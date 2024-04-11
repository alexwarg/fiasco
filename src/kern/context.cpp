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
  protected Rcu_item
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

  struct State_request
  {
    Spin_lock<> lock;
    Mword add;
    Mword del;

    bool pending() const { return access_once(&add) || access_once(&del); }
  };

protected:
  struct Kernel_drq : Drq { Context *src; };

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

  Ku_mem_ptr<Utcb> const &utcb() const
  { return _utcb; }

  /**
   * Get the queue item of the context.
   *
   * \return The queue item of the context.
   *
   * The queue item can be used to enqueue the context to a Queue.
   * a context must be in at most one queue at a time.
   * To figure out the context corresponding to a queue item
   * context_of() can be used.
   */
  Queue_item *queue_item()
  {
    return &_drq;
  }

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
  void arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load);

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


  void arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load);
  void arch_update_vcpu_state(Vcpu_state *vcpu);
  void arch_vcpu_ext_shutdown();

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

  Switch switch_handle_drq()
  {
    if (EXPECT_TRUE(home_cpu() == get_current_cpu()))
      return EXPECT_FALSE(handle_drq()) ? Switch::Resched : Switch::Ok;
    return Switch::Ok;
  }

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

private: // DRQ budle of ate
  Drq _drq;
  Drq_q _drq_q;

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
  static Per_cpu<Kernel_drq> _kernel_drq;
};


INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Context
{
public:
  struct Drq_log : public Tb_entry
  {
    void *func;
    Context *thread;
    Drq const *rq;
    Cpu_number target_cpu;
    enum class Type
    {
      Send, Do_send, Do_request, Send_reply, Do_reply, Done
    } type;
    bool wait;
    void print(String_buffer *buf) const;
    Group_order has_partner() const
    {
      switch (type)
        {
        case Type::Send: return Group_order::first();
        case Type::Do_send: return Group_order(1);
        case Type::Do_request: return Group_order(2);
        case Type::Send_reply: return Group_order(3);
        case Type::Do_reply: return Group_order(4);
        case Type::Done: return Group_order::last();
        }
      return Group_order::none();
    }

    Group_order is_partner(Drq_log const *o) const
    {
      if (rq != o->rq || func != o->func)
        return Group_order::none();

      return o->has_partner();
    }
  };


  struct Vcpu_log : public Tb_entry
  {
    Mword state;
    Mword ip;
    Mword sp;
    Mword space;
    Mword err;
    unsigned char type;
    unsigned char trap;
    void print(String_buffer *buf) const;
  };
};

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
DEFINE_PER_CPU Per_cpu<Context::Kernel_drq> Context::_kernel_drq;

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
//IMPLEMENT inline NEEDS["logdefs.h"]
PUBLIC inline NEEDS["logdefs.h"]
bool
Context::execute_drq(Drq *r, Drq_q::Drop_mode drop, bool local)
{
  bool need_resched = false;
  Context *const self = this;
  if (0)
    printf("CPU[%2u:%p]: context=%p: handle request for %p (func=%p, arg=%p)\n", cxx::int_value<Cpu_number>(current_cpu()), current(), self, r->context(), r->func, r->arg);
  if (r->context() == self)
    {
      LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
          l->type = Drq_log::Type::Do_reply;
          l->rq = r;
          l->func = (void*)r->func;
          l->thread = r->context();
          l->target_cpu = current_cpu();
          l->wait = 0;
      );
      //LOG_MSG_3VAL(current(), "hrP", current_cpu() | (drop ? 0x100: 0), (Mword)r->context(), (Mword)r->func);
      self->state.change_dirty(~Thread_drq_wait, Thread_ready);
      self->handle_remote_state_change();
      return !self->state.has(Thread_ready_mask);
    }
  else
    {
      LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
          l->type = Drq_log::Type::Do_request;
          l->rq = r;
          l->func = (void*)r->func;
          l->thread = r->context();
          l->target_cpu = current_cpu();
          l->wait = 0;
      );

      Drq::Result answer = Drq::done();
      if (EXPECT_TRUE(drop == Drq_q::No_drop && r->func))
        {
          self->handle_remote_state_change();
          answer = r->func(r, self, r->arg);
        }
      else if (EXPECT_FALSE(drop == Drq_q::Drop))
        // flag DRQ abort for requester
        r->arg = (void*)-1;

      need_resched |= answer.need_resched();

      // enqueue answer
      if (!(answer.no_answer()))
        {
          Context *c = r->context();
          if (local)
            {
              c->state.change_dirty(~Thread_drq_wait, Thread_ready);
              return need_resched;
            }
          else
            need_resched |= c->enqueue_drq(r);
        }
    }
  return need_resched;
}

IMPLEMENT
bool
Context::handle_drq()
{

  assert (check_for_current_cpu());
  assert (cpu_lock.test());

  bool resched = false;
  Mword st = state();
  if (EXPECT_FALSE(st & Thread_switch_hazards))
    {
      state.del_dirty(Thread_switch_hazards);
      if (st & Thread_finish_migration)
        finish_migration();

      if (st & Thread_need_resched)
        resched = true;
    }

  if (EXPECT_TRUE(!drq_pending()))
    return resched;

  Mem::barrier();
  resched |= _drq_q.handle_requests(this);
  state.del_dirty(Thread_drq_ready);

  //LOG_MSG_3VAL(this, "xdrq", state(), 0, cpu_lock.test());

  return resched || !(state.has(Thread_ready_mask));
}

PROTECTED inline
void
Context::handle_remote_state_change()
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

PUBLIC inline
void
Context::set_home_cpu(Cpu_number cpu)
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
 * \brief Queue a DRQ for changing the contexts state.
 * \param mask bit mask for the state (state &= mask).
 * \param add bits to add to the state (state |= add).
 * \note This function is a preemption point.
 *
 * This function must be used to change the state of contexts that are
 * potentially running on a different CPU.
 */
PUBLIC inline NEEDS["thread_state.h"]
bool
Context::xcpu_state_change(Mword mask, Mword add, bool lazy_q = false)
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


/**
 * \brief Initiate a DRQ for the context.
 * \pre \a src must be the currently running context.
 * \param src the source of the DRQ (the context who initiates the DRQ).
 * \param func the DRQ handler.
 * \param arg the argument for the DRQ handler.
 *
 * DRQs are requests that any context can queue to any other context. DRQs are
 * the basic mechanism to initiate actions on remote CPUs in an MP system,
 * however, are also allowed locally.
 * DRQ handlers of pending DRQs are executed by Context::handle_drq() in the
 * context of the target context. Context::handle_drq() is basically called
 * after switching to a context in Context::switch_exec_locked().
 *
 * This function enqueues a DRQ and blocks the current context for a reply DRQ.
 */
PUBLIC inline NEEDS["logdefs.h", "thread_state.h"]
void
Context::drq(Drq *drq, Drq::Request_func *func, void *arg,
             Drq::Wait_mode wait = Drq::Wait)
{
  if (0)
    printf("CPU[%2u:%p]: > Context::drq(this=%p, func=%p, arg=%p)\n", cxx::int_value<Cpu_number>(current_cpu()), current(), this, func,arg);
  Context *cur = current();
  LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
      l->type = Drq_log::Type::Send;
      l->rq = drq;
      l->func = (void*)func;
      l->thread = this;
      l->target_cpu = home_cpu();
      l->wait = wait;
  );
  //assert (current() == src);
  assert (!(wait == Drq::Wait && (cur->state.dirty() & Thread_drq_ready)) || cur->home_cpu() == home_cpu());
  assert (!((wait == Drq::Wait || drq == &_drq) && cur->state.dirty() & Thread_drq_wait));
  assert (!drq->queued());

  drq->func  = func;
  drq->arg   = arg;
  if (wait == Drq::Wait)
    cur->state.add(Thread_drq_wait);


  enqueue_drq(drq);

  //LOG_MSG_3VAL(src, "<drq", src->state(), Mword(this), 0);
  while (wait == Drq::Wait && cur->state.dirty() & Thread_drq_wait)
    {
      cur->state.del(Thread_ready_mask);
      cur->schedule();
    }

  LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
      l->type = Drq_log::Type::Done;
      l->rq = drq;
      l->func = (void*)func;
      l->thread = this;
      l->target_cpu = home_cpu();
      l->wait = wait;
  );
  //LOG_MSG_3VAL(src, "drq>", src->state(), Mword(this), 0);
}

PUBLIC
bool
Context::kernel_context_drq(Drq::Request_func *func, void *arg)
{
  if (EXPECT_TRUE(home_cpu() == get_current_cpu()))
    update_ready_list();

  Context *kc = kernel_context(current_cpu());
  if (current() == kc)
    return func(0, kc, arg).need_resched();

  Kernel_drq *mdrq = new (&_kernel_drq.current()) Kernel_drq;

  mdrq->src = this;
  mdrq->func  = func;
  mdrq->arg   = arg;
  kc->_drq_q.enq(mdrq);
  return schedule_switch_to_locked(kc) != Switch::Ok;
}

PUBLIC inline NEEDS[Context::drq]
void
Context::drq(Drq::Request_func *func, void *arg,
             Drq::Wait_mode wait = Drq::Wait)
{ return drq(&current()->_drq, func, arg, wait); }

PRIVATE static
bool
Context::rcu_unblock(Rcu_item *i)
{
  assert(cpu_lock.test());
  return static_cast<Context*>(i)->xcpu_state_change(~Thread_waiting, Thread_ready);
}

PUBLIC inline
void
Context::recover_jmp_buf(jmp_buf *b)
{ _recover_jmpbuf = b; }

IMPLEMENT_DEFAULT inline
void
Context::arch_load_vcpu_kern_state(Vcpu_state *, bool)
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_load_vcpu_user_state(Vcpu_state *, bool)
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_vcpu_ext_shutdown()
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_update_vcpu_state(Vcpu_state *)
{}

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

//----------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

/** logged context switch. */
class Tb_entry_ctx_sw : public Tb_entry
{
public:
  using Tb_entry::_ip;

  Context const *dst;		///< switcher target
  Context const *dst_orig;
  Address kernel_ip;
  Mword lock_cnt;
  Space const *from_space;
  Sched_context const *from_sched;
  Mword from_prio;
  void print(String_buffer *buf) const;
};



// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "kobject_dbg.h"
#include "string_buffer.h"

PUBLIC inline
Mword *
Context::get_kernel_sp() const
{
  return _kernel_sp;
}

IMPLEMENT
void
Context::Drq_log::print(String_buffer *buf) const
{
  static char const *const _types[] =
    { "send", "do send", "do request", "send reply", "do reply", "done" };

  char const *t = "unk";
  if ((unsigned)type < sizeof(_types)/sizeof(_types[0]))
    t = _types[(unsigned)type];

  buf->printf("%s(%s) rq=%p to ctxt=%lx/%p (func=%p) cpu=%u",
      t, wait ? "wait" : "no-wait", rq, Kobject_dbg::pointer_to_id(thread),
      thread, func, cxx::int_value<Cpu_number>(target_cpu));
}

// context switch
IMPLEMENT
void
Tb_entry_ctx_sw::print(String_buffer *buf) const
{
  Context *sctx = 0;
  Mword sctxid = ~0UL;
  Mword dst;
  Mword dst_orig;

  sctx = from_sched->context();
  sctxid = Kobject_dbg::pointer_to_id(sctx);

  dst = Kobject_dbg::pointer_to_id(this->dst);
  dst_orig = Kobject_dbg::pointer_to_id(this->dst_orig);

  if (sctx != ctx())
    buf->printf("(%lx)", sctxid);

  buf->printf(" ==> %lx ", dst);

  if (dst != dst_orig || lock_cnt)
    buf->printf("(");

  if (dst != dst_orig)
    buf->printf("want %lx", dst_orig);

  if (dst != dst_orig && lock_cnt)
    buf->printf(" ");

  if (lock_cnt)
    buf->printf("lck %lu", lock_cnt);

  if (dst != dst_orig || lock_cnt)
    buf->printf(") ");

  buf->printf(" krnl " L4_PTR_FMT " @ " L4_PTR_FMT, kernel_ip, _ip);
}


