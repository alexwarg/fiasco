INTERFACE:
#include <csetjmp>             // typedef jmp_buf
#include "types.h"
#include "clock.h"
#include "config.h"
#include "continuation.h"
#include "fpu_state.h"
#include "globals.h"
#include "l4_types.h"
#include "member_offs.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "queue.h"
#include "queue_item.h"
#include "rcupdate.h"
#include "sched_context.h"
#include "space.h"
#include "spin_lock.h"
#include "timeout.h"
#include <fiasco_defs.h>
#include <cxx/function>

#include <cxx/atomic>
#include <context_ptr.h>
#include <context_space_ref.h>
#include <context_vcpu_arch_base.h>
#include <context_drq.h>
#include <drq.h>
#include <drq_queue.h>
#include <globalconfig.h>

//#if defined(CONFIG_MP)
INTERFACE [mp]:
#include <context_mp.h>

template<typename C>
using Context_mp_up_x = Context_mp_x<C>;

//#else // CONFIG_MP
INTERFACE [!mp]:
#include <context_up.h>

template<typename C>
using Context_mp_up_x = Context_up_x<C>;

//#endif // CONFIG_MP
INTERFACE:

class Entry_frame;
class Context;
class Kobject_iface;

/** An execution context.  A context is a runnable, schedulable activity.
    It carries along some state used by other subsystems: A lock count,
    and stack-element forward/next pointers.
 */
class Context :
  public Context_base,
  public Context_mp_up_x<Context>,
  public Context_drq_x<Context>,
  protected Rcu_item,
  public Context_vcpu_arch_base
{
  MEMBER_OFFSET();
  friend class Jdb_thread;
  friend class Jdb_thread_list;
  friend class Jdb_utcb;
  friend class Jdb;
  friend class Jdb_tcb;

  friend class Context_ptr;
  friend class Switch_lock;
  friend Context_mp_up_x<Context>;
  friend Context_drq_x<Context>;

  struct State_request
  {
    Spin_lock<> lock;
    Mword add;
    Mword del;

    bool pending() const { return access_once(&add) || access_once(&del); }
  };

protected:
  virtual bool initiate_migration() = 0;
  virtual void finish_migration() = 0;

public:
  using Drq = ::Drq;
  using Drq_q = Drq_queue;

  struct Migration
  {
    Cpu_number cpu;
    L4_sched_param const *sp;
    bool in_progress;

    Migration() : in_progress(false) {}
  };

  template<typename T>
  class Ku_mem_ptr : public Context_member
  {
    MEMBER_OFFSET();

  private:
    typename User<T>::Ptr _u;
    T *_k;

  public:
    Ku_mem_ptr() : _u(0), _k(0) {}
    Ku_mem_ptr(typename User<T>::Ptr const &u, T *k) : _u(u), _k(k) {}

    void set(typename User<T>::Ptr const &u, T *k)
    { _u = u; _k = k; }

    T *access(bool is_current = false) const
    {
      // assert (!is_current || current() == context());
      if (is_current
          && (int)Config::Access_user_mem == Config::Access_user_mem_direct)
        return _u.get();

      Cpu_number const cpu = current_cpu();
      if ((int)Config::Access_user_mem == Config::Must_access_user_mem_direct
          && cpu == context()->home_cpu()
          && Mem_space::current_mem_space(cpu) == context()->space())
        return _u.get();
      return _k;
    }

    typename User<T>::Ptr usr() const { return _u; }
    T* kern() const { return _k; }
  };

  /**
   * Definition of different helping modes
   */
  enum Helping_mode
  {
    Helping,
    Not_Helping,
    Ignore_Helping
  };

  enum class Switch
  {
    Ok      = 0,
    Resched = 1,
    Failed  = 2
  };

  Context() noexcept
  : _kernel_sp(reinterpret_cast<Mword*>(regs()))
  {}

  virtual ~Context() noexcept = default;

  void reset_kernel_sp() noexcept
  {
    _kernel_sp = reinterpret_cast<Mword*>(regs());
  }

  Mword *get_kernel_sp() const
  {
    return _kernel_sp;
  }

  void recover_jmp_buf(jmp_buf *b)
  { _recover_jmpbuf = b; }

  Ku_mem_ptr<Utcb> const &utcb() const
  { return _utcb; }

  /**
   * \brief Check for pending DRQs.
   * \return true if there are DRQs pending, false if not.
   */
  bool drq_pending() const
  { return _drq_q.first(); }

  /**
   * \brief Handle all pending DRQs.
   * \pre cpu_lock.test() (The CPU lock must be held).
   * \pre current() == this (only the currently running context is allowed to
   *      call this function).
   * \return true if re-scheduling is needed (ready queue has changed),
   *         false if not.
   */
  bool handle_drq();

  /**
   * Return consumed CPU time.
   * @return Consumed CPU time in usecs
   */
  Cpu_time consumed_time()
  {
    if (Config::Fine_grained_cputime)
      return _clock.cpu(home_cpu()).us(_consumed_time);

    return _consumed_time;
  }

  /**
   * Add to consumed CPU time.
   * @param quantum Implementation-specific time quantum (TSC ticks or usecs)
   */
  void consume_time(Clock::Time quantum)
  {
    _consumed_time += quantum;
  }

  void spill_user_state();
  void fill_user_state();
  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const;

  [[gnu::pure]] Space *space() const { return _space.space(); }
  [[gnu::pure]] Mem_space *mem_space() const { return static_cast<Mem_space*>(space()); }

  bool migration_pending() const
  { return _migration.load(cxx::memory_order_relaxed); }

  void inc_lock_cnt()
  {
    _lock_cnt.add_fetch(1, cxx::memory_order_relaxed);
  }

  int lock_cnt() const
  {
    return _lock_cnt.load(cxx::memory_order_relaxed);
  }

  Cpu_number home_cpu() const { return _home_cpu; }

  bool check_for_current_cpu() const
  {
    Cpu_number hc = access_once(&_home_cpu);
    bool r = hc == current_cpu() || !Cpu::online(hc);
    if (0 && EXPECT_FALSE(!r)) // debug output disabled
      printf("FAIL: cpu=%u (current=%u) %p current=%p\n",
             cxx::int_value<Cpu_number>(hc),
             cxx::int_value<Cpu_number>(current_cpu()), this, current());
    return r;
  }

  bool is_invalid(bool check_cpu_local = false) const
  {
    assert(check_cpu_local || check_for_current_cpu());
    return state.is_invalid();
  }

  /**
   * Check if Context is in ready-list.
   * @return 1 if thread is in ready-list, 0 otherwise
   */
  Mword in_ready_list() const
  {
    return sched()->in_ready_list();
  }



  Context_space_ref *space_ref()
  { return &_space; }

  Space *vcpu_aware_space() const
  { return _space.vcpu_aware(); }

  Entry_frame *regs() const
  {
    return reinterpret_cast<Entry_frame *>
      (Cpu::stack_align(reinterpret_cast<Mword>(this) + Size)) - 1;
  }

  void set_home_cpu(Cpu_number cpu)
  {
    auto guard = lock_guard(_remote_state_change.lock);

    if (_remote_state_change.pending())
      {
        Mword add = access_once(&_remote_state_change.add);
        Mword del = access_once(&_remote_state_change.del);
        _remote_state_change.add = 0;
        _remote_state_change.del = 0;
        state.change_dirty(~del, add);
      }

    write_now(&_home_cpu, cpu);
  }

  /**
   * Switch active timeslice of this Context.
   * @param next Sched_context to switch to
   */
  void switch_sched(Sched_context *next, Sched_context::Ready_queue *queue)
  {
    queue->switch_sched(sched(), next);
    set_sched(next);
  }

  /**
   * Select a different context for running and activate it.
   */
  void schedule();

  /**
   * \brief Activate a newly created thread.
   *
   * This function sets a new thread onto the ready list and switches to
   * the thread if it can preempt the currently running thread.
   */
  void activate();

  void schedule_if(bool s)
  {
    if (!s || Sched_context::rq.current().schedule_in_progress)
      return;

    schedule();
  }

  /**
   * Return Context's Sched_context with id 'id'; return time slice 0 as default.
   * @return Sched_context with id 'id' or 0
   */
  Sched_context *sched_context(unsigned short const id = 0) const
  {
    if (EXPECT_TRUE (!id))
      return const_cast<Sched_context*>(&_sched_context);
    return 0;
  }

  /**
   * Return Context's currently active Sched_context.
   * @return Active Sched_context
   */
  Sched_context *sched() const
  {
    return _sched;
  }

  /**
   * Helper.  Context that helps us by donating its time to us. It is
   * set by switch_exec() if the calling thread says so.
   * @return context that helps us and should be activated after freeing a lock.
   */
  Context *helper() const
  {
    return _helper;
  }

  void set_helper(Helping_mode const mode)
  {
    switch (mode)
      {
      case Helping:
        _helper = current();
        break;
      case Not_Helping:
        _helper = this;
        break;
      case Ignore_Helping:
        // don't change _helper value
        break;
      }
  }

  void set_kernel_sp(Mword *sp)
  {
    _kernel_sp = sp;
  }

  Fpu_state *fpu_state()
  {
    return &_fpu_state;
  }

  void switch_to_locked(Context *t)
  {
    if (EXPECT_FALSE(schedule_switch_to_locked(t) != Switch::Ok))
      schedule();
  }

  bool deblock_and_schedule(Context *to)
  {
    if (Sched_context::rq.current().deblock(to->sched(), sched(), true))
      {
        switch_to_locked(to);
        return true;
      }

    return false;
  }

  /**
   * Switch to a specific different execution context.
   *        If that context is currently locked, switch to its locker instead
   *        (except if current() is the locker)
   * @pre current() == this  &&  current() != t
   * @param t thread that shall be activated.
   * @param mode helping mode; we either help, don't help or leave the
   *             helping state unchanged
   */
  FIASCO_WARN_RESULT
  Switch switch_exec_locked(Context *t, enum Helping_mode mode);

  Switch switch_exec_helping(Context *t, Mword const *lock, Mword val);

  /**
   * \brief Queue a DRQ for changing the contexts state.
   * \param mask bit mask for the state (state &= mask).
   * \param add bits to add to the state (state |= add).
   * \note This function is a preemption point.
   *
   * This function must be used to change the state of contexts that are
   * potentially running on a different CPU.
   */
  bool xcpu_state_change(Mword mask, Mword add, bool lazy_q = false)
  {
    Cpu_number current_cpu = ::current_cpu();
    if (EXPECT_FALSE(access_once(&_home_cpu) != current_cpu))
      {
        auto guard = lock_guard(_remote_state_change.lock);
        if (EXPECT_TRUE(access_once(&_home_cpu) != current_cpu))
          {
            _remote_state_change.add = (_remote_state_change.add & mask) | add;
            _remote_state_change.del = (_remote_state_change.del & ~add)  | ~mask;
            guard.reset();
            pending_rqq_enqueue();
            return false;
          }
      }

    state.change_dirty(mask, add);
    if (add & Thread_ready_mask)
      return Sched_context::rq.current().deblock(sched(), current()->sched(), lazy_q);
    return false;
  }



  // -- static fns --
  static Context *kernel_context(Cpu_number cpu)
  { return _kernel_ctxt.cpu(cpu); }

protected:
  /**
   * Update consumed CPU time during each context switch and when
   *        reading out the current thread's consumed CPU time.
   */
  void update_consumed_time()
  {
    if (Config::Fine_grained_cputime)
      consume_time(_clock.current().delta());
  }

  /**
   * Set Context's currently active Sched_context.
   * @param sched Sched_context to be activated
   */
  void set_sched(Sched_context *sched)
  {
    _sched = sched;
  }

  /**
   * Switch scheduling context and execution context.
   * @param t Destination thread whose scheduling context and execution context
   *          should be activated.
   */
  FIASCO_WARN_RESULT
  Switch schedule_switch_to_locked(Context *t)
  {
     // Must be called with CPU lock held
    assert (cpu_lock.test());

    Sched_context::Ready_queue &rq = Sched_context::rq.current();
    // Switch to destination thread's scheduling context
    if (rq.current_sched() != t->sched())
      rq.set_current_sched(t->sched());

    if (EXPECT_FALSE(t == this))
      return switch_handle_drq();

    return switch_exec_locked(t, Not_Helping);
  }

  void handle_remote_state_change()
  {
    if (!_remote_state_change.pending())
      return;

    Mword add, del;
      {
        auto guard = lock_guard(_remote_state_change.lock);
        add = access_once(&_remote_state_change.add);
        del = access_once(&_remote_state_change.del);
        _remote_state_change.add = 0;
        _remote_state_change.del = 0;
      }

    state.change_dirty(~del, add);
  }

  // -- static fns --
  static void kernel_context(Cpu_number cpu, Context *ctxt)
  { _kernel_ctxt.cpu(cpu) = ctxt; }

private:
  /// low level page table switching
  void switchin_context(Context *) asm ("switchin_context_label") FIASCO_FASTCALL;

  /// low level fpu switching
  void switch_fpu(Context *t);

  /// low level cpu switching
  void switch_cpu(Context *t);

  /**
   * Enqueue current() if ready to fix up ready-list invariant.
   */
  void update_ready_list()
  {
    assert (this == current());

    if (state.has(Thread_ready_mask) && sched()->left())
      Sched_context::rq.current().ready_enqueue(sched());
  }

  // update the ready list after a DRQ
  bool update_ready_list_drq(bool resched, bool offline_cpu = false)
  {
    // migrated awy, to a non-offlien CPU, so we are done
    if (EXPECT_FALSE(!offline_cpu && home_cpu() != current_cpu()))
      return false;

    // already in ready list or not ready, done and pass resched
    if (in_ready_list() || !state.has(Thread_ready_mask))
      return resched;

    // need to enqueue, on foreign CPU if offline, on current CPU else
    if (EXPECT_FALSE(offline_cpu))
      Sched_context::rq.cpu(home_cpu()).ready_enqueue(sched());
    else
      Sched_context::rq.current().ready_enqueue(sched());

    // need to reschedule in this case
    return true;
  }

  // execute DRQ and update ready list according to new state
  bool do_drq(Drq *rq, bool offline_cpu = false)
  {
    return update_ready_list_drq(execute_drq(rq, Drq_queue::No_drop, true),
                                 offline_cpu);
  }

  // handle a DRQ in switch_exec*, DRQs are only handled if
  // executing on the home CPU
  Switch switch_handle_drq()
  {
    if (EXPECT_TRUE(home_cpu() == get_current_cpu()))
      return EXPECT_FALSE(handle_drq()) ? Switch::Resched : Switch::Ok;
    return Switch::Ok;
  }


  // -- static fns --
  static bool rcu_unblock(Rcu_item *i);

protected:
  Cpu_number _home_cpu = Cpu::invalid();
  Mword *_kernel_sp;
  Context_space_ref _space;

private:
  // The scheduling parameters.  We would only need to keep an
  // anonymous reference to them as we do not need them ourselves, but
  // we aggregate them for performance reasons.
  Sched_context _sched_context;
  Sched_context *_sched = &_sched_context;
  // Implementation-specific consumed CPU time (TSC ticks or usecs)
  Clock::Time _consumed_time;

  // Pointer to floating point register state
  Fpu_state _fpu_state;

protected:
  // XXX Timeout for both, sender and receiver! In normal case we would have
  // to define own timeouts in Receiver and Sender but because only one
  // timeout can be set at one time we use the same timeout. The timeout
  // has to be defined here because Dirq::hit has to be able to reset the
  // timeout (Irq::_irq_thread is of type Receiver).
  Timeout *_timeout;
  void *_utcb_handler;
  Ku_mem_ptr<Utcb> _utcb;

private:
  Context *_helper = this;

  // Lock state
  // how many locks does this thread hold on other threads
  // incremented in Thread::lock, decremented in Thread::clear
  // Thread::kill needs to know
  cxx::atomic<int> _lock_cnt;

protected:
  cxx::atomic<Migration *> _migration;

private:
  State_request _remote_state_change;

protected:
  // for trigger_exception
  Continuation _exc_cont;
  jmp_buf *_recover_jmpbuf;     // setjmp buffer for page-fault recovery

private:
  static Per_cpu<Clock> _clock;
  static Per_cpu<Context *> _kernel_ctxt;
};


INTERFACE [debug]:

#include <drq_log.h>
#include <vcpu_log.h>
#include <context_dbg.h>

// --------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include <cxx/atomic>

#include "cpu.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "fpu.h"
#include "globals.h"		// current()
#include "lock_guard.h"
#include "logdefs.h"
#include "mem.h"
#include "mem_layout.h"
#include "processor.h"
#include "space.h"
#include "std_macros.h"
#include "thread_state.h"
#include "timer.h"
#include "timeout.h"
#include "assert.h"

DEFINE_PER_CPU Per_cpu<Clock> Context::_clock(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU Per_cpu<Context *> Context::_kernel_ctxt;

IMPLEMENT
void
Context::schedule()
{
  auto guard = lock_guard(cpu_lock);
  assert (!Sched_context::rq.current().schedule_in_progress);

  // we give up the CPU as a helpee, so we have no helper anymore
  if (EXPECT_FALSE(helper() != this))
    set_helper(Not_Helping);

  // if we are a thread on a foreign CPU we must ask the kernel context to
  // schedule for us
  Cpu_number current_cpu = ::current_cpu();
  while (EXPECT_FALSE(current_cpu != access_once(&_home_cpu)))
    {
      Context *kc = Context::kernel_context(current_cpu);
      assert (this != kc);

      // flag that we need to schedule
      kc->state.add_dirty(Thread_need_resched);
      switch (switch_exec_locked(kc, Ignore_Helping))
        {
        case Switch::Ok:
          return;
        case Switch::Resched:
          current_cpu = ::current_cpu();
          continue;
        case Switch::Failed:
          assert (false);
          continue;
        }
    }

  // now, we are sure that a thread on its home CPU calls schedule.
  CNT_SCHEDULE;

  // Ensure only the current thread calls schedule
  assert (this == current());

  Sched_context::Ready_queue *rq = &Sched_context::rq.current();

  // Enqueue current thread into ready-list to schedule correctly
  update_ready_list();

  // Select a thread for scheduling.
  Context *next_to_run;

  for (;;)
    {
      next_to_run = rq->next_to_run()->context();

      // Ensure ready-list sanity
      assert (next_to_run);

      if (EXPECT_FALSE(!next_to_run->state.has(Thread_ready_mask)))
        rq->ready_dequeue(next_to_run->sched());
      else switch (schedule_switch_to_locked(next_to_run))
        {
        default:
        case Switch::Ok:      return;   // ok worked well
        case Switch::Failed:  break;    // not migrated, need preemption point
        case Switch::Resched:
          {
            Cpu_number n = ::current_cpu();
            if (n != current_cpu)
              {
                current_cpu = n;
                rq = &Sched_context::rq.current();
              }
          }
          continue; // may have been migrated...
        }

      rq->schedule_in_progress = this;
      Proc::preemption_point();
      if (EXPECT_TRUE(current_cpu == ::current_cpu()))
        rq->schedule_in_progress = 0;
      else
        return; // we got migrated and selected on our new CPU, so we may run
    }
}

IMPLEMENT
void
Context::activate()
{
  auto guard = lock_guard(cpu_lock);
  if (xcpu_state_change(~0UL, Thread_ready, true))
    current()->switch_to_locked(this);
}

// queue operations

// XXX for now, synchronize with global kernel lock
//-
IMPLEMENT
Context::Switch
Context::switch_exec_locked(Context *t, enum Helping_mode mode)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // Time-slice lending: if t is locked, switch to its locker
  // instead, this is transitive
  //

  if (EXPECT_FALSE(t->running_on_different_cpu()))
    {
      if (!t->in_ready_list())
        Sched_context::rq.current().ready_enqueue(t->sched());
      return Switch::Failed;
    }


  LOG_CONTEXT_SWITCH;
  CNT_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!t->state.has(Thread_ready_mask)))
    {
      assert (state.has(Thread_ready_mask));
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_kernel_sp);

  t->set_helper(mode);

  if (EXPECT_TRUE(get_current_cpu() == home_cpu()))
    update_ready_list();

  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);

  return switch_handle_drq();
}

IMPLEMENT
Context::Switch
Context::switch_exec_helping(Context *t, Mword const *lock, Mword val)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // we actually hold locks
  if (!t->need_help(lock, val))
    return Switch::Failed;

  LOG_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!t->state.has(Thread_ready_mask)))
    {
      assert (state.has(Thread_ready_mask));
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_kernel_sp);

  if (EXPECT_TRUE(get_current_cpu() == home_cpu()))
    update_ready_list();

  t->set_helper(Helping);
  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);
  return switch_handle_drq();
}



IMPLEMENT
bool
Context::rcu_unblock(Rcu_item *i)
{
  assert(cpu_lock.test());
  return static_cast<Context*>(i)->xcpu_state_change(~Thread_waiting, Thread_ready);
}

IMPLEMENT_DEFAULT inline
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{ dst->copy_and_sanitize(src); }

//----------------------------------------------------------------------------
IMPLEMENTATION [fpu && lazy_fpu]:

#include "assert.h"
#include "fpu.h"

PUBLIC inline NEEDS ["fpu.h"]
void
Context::spill_fpu()
{
  // If we own the FPU, we should never be getting an "FPU unavailable" trap
  assert (Fpu::fpu.current().owner() == this);
  assert (state.has(Thread_fpu_owner));
  assert (fpu_state());

  // Save the FPU state of the previous FPU owner (lazy) if applicable
  Fpu::save_state(fpu_state());
  state.del_dirty(Thread_fpu_owner);
}

/**
 * When switching away from the FPU owner, disable the FPU to cause
 * the next FPU access to trap.
 * When switching back to the FPU owner, enable the FPU so we don't
 * get an FPU trap on FPU access.
 */
IMPLEMENT inline NEEDS ["fpu.h"]
void
Context::switch_fpu(Context *t)
{
  Fpu &f = Fpu::fpu.current();
  if (f.is_owner(this))
    f.disable();
  else if (f.is_owner(t) && !t->state.has(Thread_vcpu_fpu_disabled))
    f.enable();
}

PUBLIC inline NEEDS["fpu.h"]
void
Context::spill_fpu_if_owner()
{
  // spill FPU state into memory before migration
  if (!state.has(Thread_fpu_owner))
    return;

  Fpu &f = Fpu::fpu.current();

  if (current() != this)
    f.enable();

  spill_fpu();
  f.set_owner(0);
  f.disable();
}

PUBLIC static
void
Context::spill_current_fpu(Cpu_number cpu)
{
  (void)cpu;
  assert (cpu == current_cpu());

  Fpu &f = Fpu::fpu.current();
  if (f.owner())
    {
      f.enable();
      f.owner()->spill_fpu();
      f.set_owner(0);
      f.disable();
    }
}


PUBLIC inline NEEDS["fpu.h"]
void
Context::release_fpu_if_owner()
{
  // If this context owns the FPU, no one owns it now
  Fpu &f = Fpu::fpu.current();
  if (f.is_owner(this))
    {
      f.set_owner(0);
      f.disable();
    }
}

//----------------------------------------------------------------------------
IMPLEMENTATION [fpu && !lazy_fpu]:

#include "fpu.h"

PUBLIC inline NEEDS ["fpu.h"]
void
Context::spill_fpu()
{
  assert (fpu_state());

  // Save the FPU state of the previous FPU owner
  Fpu::save_state(fpu_state());
}

IMPLEMENT inline NEEDS ["fpu.h"]
void
Context::switch_fpu(Context *t)
{
  Fpu &f = Fpu::fpu.current();

  if (state.has(Thread_vcpu_fpu_disabled))
    f.enable();

  spill_fpu();
  f.restore_state(t->fpu_state());

  if (t->state.has(Thread_vcpu_fpu_disabled))
    f.disable();
}

PUBLIC inline
void
Context::spill_fpu_if_owner()
{
  if (current() != this)
    return;

  spill_fpu();
}

PUBLIC static
void
Context::spill_current_fpu(Cpu_number cpu)
{
  (void)cpu;
  assert (cpu == current_cpu());

  current()->spill_fpu();
}


PUBLIC inline
void
Context::release_fpu_if_owner()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [!fpu]:

PUBLIC inline
void
Context::spill_fpu_if_owner()
{}

PUBLIC static
void
Context::spill_current_fpu(Cpu_number)
{}

PUBLIC inline
void
Context::spill_fpu()
{}

PUBLIC inline
void
Context::release_fpu_if_owner()
{}

IMPLEMENT inline
void
Context::switch_fpu(Context *)
{}

