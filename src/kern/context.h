#pragma once
#include <csetjmp>             // typedef jmp_buf
#include "types.h"
#include "clock.h"
#include "config.h"
#include "continuation.h"
#include "fpu_state_ptr.h"
#include "globals.h"
#include "l4_types.h"
#include "member_offs.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "queue.h"
#include "queue_item.h"
#include "rcupdate.h"
#include <receiver.h>
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
#include <context_vcpu.h>
#include <context_arch.h>
#include <context_cpu_state.h>
#include <context_migration.h>
#include <drq.h>
#include <drq_queue.h>
#include <ku_mem_ptr.h>
#include <globalconfig.h>

#ifdef CONFIG_JDB
#include <vcpu_log.h>
#include <drq_log.h>
#include <context_dbg.h>
#endif

#ifdef CONFIG_FPU
#  ifdef CONFIG_LAZY_FPU
#    include <context_lazy_fpu.h>
#  else
#    include <context_eager_fpu.h>
#  endif
#else
#  include <context_no_fpu.h>
#endif

#if defined(CONFIG_MP)
#include <context_mp.h>

template<typename C>
using Context_mp_up_x = Context_mp_x<C>;

#else // CONFIG_MP
#include <context_up.h>

template<typename C>
using Context_mp_up_x = Context_up_x<C>;

#endif // CONFIG_MP

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
  public Context_arch_x<Context>,
  public Context_drq_x<Context>,
  protected Rcu_item,
  public Context_fpu_x<Context>,
  public Context_vcpu_x<Context>,
  public Receiver<Context>
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
  friend Context_vcpu_x<Context>;
  friend Context_arch_x<Context>;

  template<typename>
  friend class Sched;

  struct State_request
  {
    Spin_lock<> lock{Spin_lock<>::Unlocked};
    cxx::atomic<Mword> flags{0};

    bool pending() const { return flags.load(); }
  };

public:
  using Drq = ::Drq;
  using Drq_q = Drq_queue;
  using Migration = Ctxt::Migration;
  using Migration_ptr = Ctxt::Migration_ptr;


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

  Context() noexcept = default;

  virtual ~Context() noexcept = default;

  void reset_kernel_sp() noexcept
  {
    _cpu_state.kernel_sp = reinterpret_cast<Mword*>(regs());
  }

  Mword *get_kernel_sp() const
  {
    return _cpu_state.kernel_sp;
  }

  void recover_jmp_buf(jmp_buf *b)
  { _recover_jmpbuf = b; }

  void do_recover_jmp_buf()
  {
    if (_recover_jmpbuf)
      longjmp(*_recover_jmpbuf, 1);
  }

  Ku_mem_ptr<Utcb> const &utcb() const
  { return _utcb; }

  /**
   * \brief Check for pending DRQs.
   * \return true if there are DRQs pending, false if not.
   */
  bool drq_pending() const
  { return _drq_q.first(); }

  /**
   * Handle all pending DRQs for this context.
   *
   * \pre The CPU lock must be held: `cpu_lock.test()`
   * \pre The context must be either on the current CPU or on an offline CPU:
   *      `current_cpu() == home_cpu() || !Cpu::online(home_cpu())`
   * \pre If `!Cpu::online(home_cpu())`, the lock of the home CPU's _pending_rqq
   *      must be held: `_pending_rqq.cpu(home_cpu()).q_lock()->test()`
   *
   * \return True if re-scheduling is needed (ready queue has changed),
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

  [[gnu::pure]] Space *space() const { return _cpu_state.space.space(); }
  [[gnu::pure]] Mem_space *mem_space() const { return static_cast<Mem_space*>(space()); }

  bool migration_pending() const
  { return _migration.pending(); }

  void inc_lock_cnt()
  {
    _lock_cnt.add_fetch(1, cxx::memory_order_relaxed);
  }

  int lock_cnt() const
  {
    return _lock_cnt.load(cxx::memory_order_relaxed);
  }

  Cpu_number home_cpu() const { return _home_cpu; }
  Cpu_number atomic_home_cpu() const
  {
    Cpu_number n;
    __atomic_load(&_home_cpu, &n, (int)cxx::memory_order_seq_cst);
    return n;
  }

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

  bool is_invalid(bool check_cpu_local = true) const
  {
    (void) check_cpu_local;
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
  { return &_cpu_state.space; }

  Space *vcpu_aware_space() const
  { return _cpu_state.space.vcpu_aware(); }

  Entry_frame *regs() const
  {
    return reinterpret_cast<Entry_frame *>
      (Cpu::stack_align(reinterpret_cast<Mword>(this) + Size)) - 1;
  }

  void set_home_cpu(Cpu_number cpu)
  {
    auto guard = lock_guard(_remote_state_change.lock);
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
   * Return Context's Sched_context
   * @return Sched_context
   */
  Sched_context *sched_context() const
  {
    return const_cast<Sched_context*>(&_sched_context);
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
    _cpu_state.kernel_sp = sp;
  }

  Fpu_state_ptr &fpu_state()
  {
    return _fpu_state;
  }

  /**
   * Attempt to switch to the target context and reschedule on failure.
   *
   * \param t  Target context to switch to.
   *
   * \pre The CPU lock must be held (hence the _locked suffix).
   */
  void switch_to_locked(Context *t)
  {
    if (EXPECT_FALSE(schedule_switch_to_locked(t) != Switch::Ok))
      schedule();
  }

  /**
   * Deblock and switch to the target context.
   *
   * Deblock the target context's scheduling context and switch to the target
   * context if it can preempt the current context.
   *
   * \param to  Target context to be deblocked and switched to.
   *
   * \returns Whether the target context preempted the current context, i.e. a
   *          switch to the target context was attempted.
   */
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
  [[nodiscard]]
  Switch switch_exec_locked(Context *t, enum Helping_mode mode);

  Switch switch_exec_helping(Context *t, cxx::atomic<Mword> const *lock, Mword val);

  /**
   * Ready queue a potentially remote context.
   *
   * \param lazy_q  If true and the context is on the current CPU and has the higest
   *                priority it will not be enqueued, instead true is returned and
   *                the caller is responsible for switching to this context.
   *                If true the context will be queued and the result indicates if
   *                local scheduling is required.
   */
  bool xcpu_ready_enqueue(bool lazy_q = false)
  {
    if (EXPECT_FALSE(access_once(&_home_cpu) == current_cpu()))
      return Sched_context::rq.current().deblock(sched(), current()->sched(), lazy_q);

    pending_rqq_enqueue();
    return false;
  }

  /**
   * Lazyly ready queue a potentially remote context.
   *
   * If the context is on the current CPU and has the higest priority it
   * will not be enqueued, instead true is returned and the caller is
   * responsible for switching to this context.
   */
  bool xcpu_lazy_ready_enqueue()
  {
    return xcpu_ready_enqueue(true);
  }

  /**
   * Ready queue a context on another CPU.
   */
  void remote_ready_enqueue()
  {
    pending_rqq_enqueue();
  }

  void set_xcpu_ipc_pending()
  {
    Mword f = _remote_state_change.flags.fetch_or(2);
    if (!(f & 2))
      pending_rqq_enqueue();
  }


  void set_timeout(Timeout *t)
  {
    _timeout = t;
  }

  void set_timeout(Timeout *t, Unsigned64 tval)
  {
    _timeout = t;
    t->set(tval, home_cpu());
  }

  void enqueue_timeout_again()
  {
    if (_timeout && Cpu::online(home_cpu()))
      _timeout->set_again(home_cpu());
  }

  void reset_timeout()
  {
    if (EXPECT_TRUE(!_timeout))
      return;

    _timeout->reset();
    _timeout = nullptr;
  }

  bool have_timeout() const
  { return _timeout != nullptr; }

  /**
   * Update consumed CPU time during each context switch and when
   *        reading out the current thread's consumed CPU time.
   */
  void update_consumed_time()
  {
    if (Config::Fine_grained_cputime)
      consume_time(_clock.current().delta());
  }

  Continuation *cont() { return &_exc_cont; }
  Continuation const *cont() const { return &_exc_cont; }

  // -- static fns --
  static Context *kernel_context(Cpu_number cpu)
  { return _kernel_ctxt.cpu(cpu); }

protected:
  void finish_migration()
  {
    enqueue_timeout_again();
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
  [[nodiscard]]
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

    Mword flags = _remote_state_change.flags.fetch_and(0);

    if (!flags)
      return;

    if (flags & 2)
      try_vcpu_irq_receive(state());
  }

  // -- static fns --
  static void kernel_context(Cpu_number cpu, Context *ctxt)
  { _kernel_ctxt.cpu(cpu) = ctxt; }

private:
  /// low level page table switching
  void switchin_context(Context *) asm ("switchin_context_label") FIASCO_FASTCALL;
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
    return update_ready_list_drq(execute_drq(rq, true),
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

private:
  // The scheduling parameters.  We would only need to keep an
  // anonymous reference to them as we do not need them ourselves, but
  // we aggregate them for performance reasons.
  Sched_context _sched_context;
  Sched_context *_sched = &_sched_context;
  // Implementation-specific consumed CPU time (TSC ticks or usecs)
  Clock::Time _consumed_time{0};

  // Pointer to floating point register state
  Fpu_state_ptr _fpu_state;

  // XXX Timeout for both, sender and receiver! In normal case we would have
  // to define own timeouts in Receiver and Sender but because only one
  // timeout can be set at one time we use the same timeout. The timeout
  // has to be defined here because Dirq::hit has to be able to reset the
  // timeout (Irq::_irq_thread is of type Receiver).
  Timeout *_timeout = nullptr;

protected:
  Ku_mem_ptr<Utcb> _utcb;

private:
  Context *_helper = this;

  // Lock state
  // how many locks does this thread hold on other threads
  // incremented in Thread::lock, decremented in Thread::clear
  // Thread::kill needs to know
  cxx::atomic<int> _lock_cnt{0};

  Migration_ptr _migration;
  State_request _remote_state_change;

protected:
  // for trigger_exception
  Continuation _exc_cont;
  jmp_buf *_recover_jmpbuf = nullptr;     // setjmp buffer for page-fault recovery

private:
  static Per_cpu<Clock> _clock;
  static Per_cpu<Context *> _kernel_ctxt;
};

