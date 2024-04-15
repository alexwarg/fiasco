INTERFACE [fpu && lazy_fpu]:

#include <thread_lazy_fpu.h>

INTERFACE [fpu && !lazy_fpu]:

#include <thread_eager_fpu.h>

INTERFACE [!fpu]:

#include <thread_no_fpu.h>

INTERFACE:

#include <cxx/atomic>

#include "l4_types.h"
#include "config.h"
#include "continuation.h"
#include "helping_lock.h"
#include "irq_chip.h"
#include "kobject.h"
#include "mem_layout.h"
#include "member_offs.h"
#include "receiver.h"
#include "ref_obj.h"
#include "sender.h"
#include "spin_lock.h"

#include <kmem_alloc.h>
#include <task.h>
#include <kernel_task.h>
#include <irq_chip.h>
#include <sched.h>
#include <timer.h>
#include <thread_ipc.h>
#include <thread_arch.h>

class Return_frame;
class Syscall_frame;
class Task;
class Thread;
class Vcpu_state;
class Irq_base;

/** A thread.  This class is the driver class for most kernel functionality.
 */
class Thread :
  public Receiver,
  public Thread_ipc<Thread>,
  public cxx::Dyn_castable<Thread, Kobject>,
  public Thread_fpu_x<Thread>,
  public Thread_arch_x<Thread>
{
  MEMBER_OFFSET();

  friend Thread_ipc<Thread>;

  friend class Jdb;
  friend class Jdb_bt;
  friend class Jdb_tcb;
  friend class Jdb_thread;
  friend class Jdb_thread_list;
  friend class Jdb_list_threads;
  friend class Jdb_list_timeouts;
  friend class Jdb_tbuf_show;
  friend struct Utest; // do_leave_and_kill_myself()
  friend class Scheduler_test; // do_leave_and_kill_myself()

public:
  enum Context_mode_kernel { Kernel = 0 };
  enum Operation
  {
    Opcode_mask = 0xffff,
    Op_control = 0,
    Op_ex_regs = 1,
    Op_switch  = 2,
    Op_stats   = 3,
    Op_vcpu_resume = 4,
    Op_register_del_irq = 5,
    Op_modify_senders = 6,
    Op_vcpu_control = 7,
    Op_gdt_x86 = 0x10,
    Op_set_tpidruro_arm = 0x10,
    Op_set_segment_base_amd64 = 0x12,
    Op_segment_info_amd64 = 0x13,
  };

  enum Control_flags
  {
    Ctl_set_pager       = 0x0010000,
    Ctl_bind_task       = 0x0200000,
    Ctl_alien_thread    = 0x0400000,
    Ctl_set_exc_handler = 0x1000000,
  };

  enum Ex_regs_flags
  {
    Exr_cancel            = 0x10000,
    Exr_trigger_exception = 0x20000,
  };

  enum Vcpu_ctl_flags
  {
    Vcpu_ctl_extended_vcpu = 0x10000,
  };

public:
  typedef void (Utcb_copy_func)(Thread *sender, Thread *receiver);

  void *operator new(size_t, Ram_quota *q) noexcept
  {
    void *t = Kmem_alloc::allocator()->q_alloc(q, Bytes(Thread::Size));
    if (t)
      memset(t, 0, sizeof(Thread));

    return t;
  }

  // construct a thread
  explicit Thread(Ram_quota *);

  virtual ~Thread() noexcept;

  Ram_quota *quota() const
  {
    return _quota;
  }

  void set_vcpu_user_space(Task *t)
  {
    assert (current() == this);
    if (t)
      t->inc_ref();

    Task *old = static_cast<Task*>(_cpu_state.space.vcpu_user());
    _cpu_state.space.vcpu_user(t);

    if (old && !old->dec_ref())
      delete old;
  }

  // bind a thread to a kernel task t
  void kbind(Task *t)
  {
    auto guard = lock_guard(_cpu_state.space.lock());
    _cpu_state.space.space(t);
    t->inc_ref();
  }

  // bind the thread to a task (incl. UTCB)
  bool bind(Task *t, User<Utcb>::Ptr utcb);

  // unbind the thread from its task
  void unbind();

  // register an IRQ to be triggered for IPC gate deletion
  bool register_delete_irq(Irq_base *irq);

  // unregister the del IRQ
  void unregister_delete_irq();

  void remove_delete_irq(Irq_base *irq)
  {
    _del_observer.compare_exchange_strong(irq, (Irq_base *)nullptr);
  }

  void ipc_gate_deleted(Mword id)
  {
    (void) id;
    auto g = lock_guard(cpu_lock);
    if (auto irq = _del_observer.load(cxx::memory_order_relaxed))
      irq->hit(0);
  }



  bool exception_triggered() const
  {
    return _exc_cont.valid(regs());
  }

  Mword user_ip() const
  { return _exc_cont.cond_ip(regs()); }

  void user_ip(Mword ip)
  { _exc_cont.cond_ip(regs(), ip); }

  Mword user_sp() const
  { return _exc_cont.cond_sp(regs()); }

  void user_sp(Mword sp)
  { _exc_cont.cond_sp(regs(), sp); }

  bool continuation_test_and_restore()
  {
    bool v = _exc_cont.valid(regs());
    if (v)
      _exc_cont.restore(regs());
    return v;
  }

  void handle_timer_interrupt()
  {
    Cpu_number _cpu = current_cpu();
    // XXX: This assumes periodic timers (i.e. bogus in one-shot mode)
    if (!Config::Fine_grained_cputime)
      consume_time(Config::Scheduler_granularity);

    bool resched = Rcu::do_pending_work(_cpu);

    // Check if we need to reschedule due to timeouts or wakeups
    if ((Timeout_q::timeout_queue.cpu(_cpu).do_timeouts() || resched)
        && !Sched_context::rq.current().schedule_in_progress)
      {
        schedule();
        assert (timeslice_timeout.cpu(current_cpu())->is_set());	// Coma check
      }
  }

  static Drq::Result handle_kill_helper(Drq *src, Context *, void *);
  static Drq::Result handle_migration_helper(Drq *rq, Context *, void *p);

  void put_n_reap(Kobject ***reap_list);

  bool kill();

  long control(Thread_ptr const &pager, Thread_ptr const &exc_handler);

  bool check_sys_ipc(unsigned flags, Thread **partner, Thread **sender,
                     bool *have_recv) const
  {
    if (flags & L4_obj_ref::Ipc_recv)
      {
        *sender = flags & L4_obj_ref::Ipc_open_wait ? 0 : const_cast<Thread*>(this);
        *have_recv = true;
      }

    if (flags & L4_obj_ref::Ipc_send)
      *partner = const_cast<Thread*>(this);

    // FIXME: shall be removed flags == 0 is no-op
    if (!flags)
      {
        *sender = const_cast<Thread*>(this);
        *partner = const_cast<Thread*>(this);
        *have_recv = true;
      }

    return *have_recv || ((flags & L4_obj_ref::Ipc_send) && *partner);
  }

  /**
   * Setup a IPC-like timer for the given timeout.
   * \param timeout  The L4 ABI timeout value that shall be used
   * \param utcb     The UTCB that might contain an absolute timeout
   * \param timer    The timeout/timer object that shall be queued.
   *
   * This function does nothing if the timeout is *never*.
   * Sets Thread_ready and Thread_timeout in the thread state
   * if the timeout is zero or has already hit (is in the past).
   * Or enqueues the given timer object with the finite timeout calculated
   * from `timeout`.
   */
  void setup_timer(L4_timeout timeout, Utcb const *utcb, Timeout *timer)
  {
    if (EXPECT_TRUE(timeout.is_never()))
      return;

    if (EXPECT_FALSE(timeout.is_zero()))
      {
        state.add_dirty(Thread_ready | Thread_timeout);
        return;
      }

    assert (!have_timeout());

    Unsigned64 sysclock = Timer::system_clock();
    Unsigned64 tval = timeout.microsecs(sysclock, utcb);

    if (EXPECT_TRUE((tval > sysclock)))
      set_timeout(timer, tval);
    else // timeout already hit
      state.add_dirty(Thread_ready | Thread_timeout);
  }


#ifdef CONFIG_JDB
  void halt();
#endif

  // --- static fns -----
  static void assert_irq_entry()
  {
    assert(Sched_context::rq.current().schedule_in_progress
           || current()->state.has(  Thread_ready_mask
                                   | Thread_drq_wait
                                   | Thread_waiting
                                   | Thread_ipc_transfer));
  }

  [[noreturn]] static void system_abort();

private:
  void *operator new(size_t);	///< Default new operator undefined

  bool do_kill();

  /**
   * Return to user.
   *
   * This function is the default routine run if a newly
   * initialized context is being switch_exec()'ed.
   */
  static void user_invoke() FIASCO_NORETURN;

  static void do_leave_and_kill_myself() asm("thread_do_leave_and_kill_myself");
  static Drq::Result handle_remote_kill(Drq *, Context *self, void *);

  static void user_invoke_generic()
  {
    Context *const c = current();
    assert (c->state() & Thread_ready_mask);

    if (c->handle_drq())
      c->schedule();

    // release CPU lock explicitly, because
    // * the context that switched to us holds the CPU lock
    // * we run on a newly-created stack without a CPU lock guard
    cpu_lock.clear();
  }

  void prepare_kill()
  {
    extern void  FIASCO_NORETURN leave_and_kill_myself() asm ("leave_and_kill_myself");

    if (state() & (Thread_dying | Thread_dead))
      return;

    inc_ref();
    state.add_dirty(Thread_dying | Thread_cancel | Thread_ready);
    _exc_cont.restore(regs()); // overwrite an already triggered exception
    do_trigger_exception(regs(), (void*)&leave_and_kill_myself);
  }

  static void print_page_fault_error(Mword e);

public:
  /** nesting level in debugger (always critical) if >1 */
  static Per_cpu<unsigned long> nested_trap_recover;
  static void handle_global_remote_requests_irq() asm ("ipi_remote_call");

protected:
  /**
   * Cut-down version of Thread constructor; only for kernel threads
   * Do only what's necessary to get a kernel thread started --
   * skip all fancy stuff, no locking is necessary.
   */
  explicit Thread(Ram_quota *q, Context_mode_kernel)
  : _quota(q)
  {
    inc_ref();
    _cpu_state.space.space(Kernel_task::kernel_task());

    if (Config::Stack_depth)
      std::memset((char*)this + sizeof(Thread), '5',
                  Thread::Size-sizeof(Thread) - 64);

    alloc_eager_fpu_state();
  }

protected:
  Ram_quota *_quota;
  cxx::atomic<Irq_base *> _del_observer{nullptr};

  constexpr static unsigned magic = 0xf001c001;
  // Debugging facilities
  unsigned _magic = magic;

public:
#ifdef CONFIG_JDB
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
#endif
};


IMPLEMENTATION:

#include <cassert>
#include <cstdlib>		// panic()
#include <cstring>
#include <cxx/atomic>
#include "entry_frame.h"
#include "globals.h"
#include "irq_chip.h"
#include "kdb_ke.h"
#include "kernel_task.h"
#include "kmem_alloc.h"
#include "logdefs.h"
#include "map_util.h"
#include "ram_quota.h"
#include "sched_context.h"
#include "space.h"
#include "std_macros.h"
#include "task.h"
#include "thread_state.h"
#include "timeout.h"

JDB_DEFINE_TYPENAME(Thread,  "\033[32mThread\033[m");
DEFINE_PER_CPU Per_cpu<unsigned long> Thread::nested_trap_recover;

/** Currently executing thread.
    @return currently executing thread.
 */
inline
Thread*
current_thread()
{ return nonull_static_cast<Thread*>(current()); }

IMPLEMENT
Thread::Thread(Ram_quota *q)
: _quota(q)
{
  assert (state() == 0);

  inc_ref();
  _cpu_state.space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char*)this + sizeof(Thread), '5',
		Thread::Size-sizeof(Thread)-64);

  prepare_switch_to(&user_invoke);

  init_regs(regs());

  alloc_eager_fpu_state();

  state.add_dirty(Thread_dead);
  // ok, we're ready to go!
}

IMPLEMENT
bool
Thread::bind(Task *t, User<Utcb>::Ptr utcb)
{
  // _utcb == 0 for all kernel threads
  Space::Ku_mem const *u = t->find_ku_mem(utcb, sizeof(Utcb));

  if (EXPECT_FALSE(!u))
    return false;

  auto guard = lock_guard(_cpu_state.space.lock());
  if (_cpu_state.space.space() != Kernel_task::kernel_task())
    return false;

  _cpu_state.space.space(t);
  t->inc_ref();

  _utcb.set(utcb, u->kern_addr(utcb));
  arch_setup_utcb_ptr();
  return true;
}


IMPLEMENT
void
Thread::unbind()
{
  assert(   (!(state() & Thread_dead) && current() == this)
         || ( (state() & Thread_dead) && current() != this));

  Task *old;

    {
      auto guard = lock_guard(_cpu_state.space.lock());

      if (_cpu_state.space.space() == Kernel_task::kernel_task())
        return;

      old = static_cast<Task*>(_cpu_state.space.space());
      _cpu_state.space.space(Kernel_task::kernel_task());

      // switch to a safe page table if the thread is to be going itself
      if (   current() == this
          && Mem_space::current_mem_space(current_cpu()) == old)
        Kernel_task::kernel_task()->switchin_context(old);

      if (old->dec_ref())
        old = 0;
    }

  if (old)
    delete old;
}


/** Destructor.  Reestablish the Context constructor's precondition.
    @pre state() == Thread_dead
    @pre lock_cnt() == 0
    @post (_kernel_sp == 0)  &&  (* (stack end) == 0)  &&  !exists()
 */
IMPLEMENT
Thread::~Thread()		// To be called in locked state.
{
  // Thread::do_kill() already unregistered deletion IRQ, but in the meantime a
  // deletion IRQ might have been bound again.
  unregister_delete_irq();

  unsigned long *init_sp = reinterpret_cast<unsigned long*>
    (reinterpret_cast<unsigned long>(this) + Size - sizeof(Entry_frame));

  _cpu_state.kernel_sp = 0;
  *--init_sp = 0;
  free_fpu_state();
  assert (!in_ready_list());
}

// IPC-gate deletion stuff ------------------------------------

/**
 * Fake IRQ Chip class for IPC-gate-delete notifications.
 * This chip uses the IRQ number as thread pointer and implements
 * the bind and unbind functionality.
 */
class Del_irq_chip : public Irq_chip_soft
{
public:
  static Del_irq_chip chip;

  static Thread *thread(Mword pin)
  { return (Thread*)pin; }

  static Mword pin(Thread *t)
  { return (Mword)t; }

  void unbind(Irq_base *irq) override
  { thread(irq->pin())->remove_delete_irq(irq); }

};

Del_irq_chip Del_irq_chip::chip;

IMPLEMENT
bool
Thread::register_delete_irq(Irq_base *irq)
{
  if (_del_observer.load(cxx::memory_order_relaxed))
    return false;

  auto g = lock_guard(irq->irq_lock());
  irq->unbind();
  Del_irq_chip::chip.bind(irq, (Mword)this);
  Irq_base *none = nullptr;
  if (_del_observer.compare_exchange_strong(none, irq))
    return true;

  irq->unbind();
  return false;
}

IMPLEMENT
void
Thread::unregister_delete_irq()
{
  auto irq = _del_observer.load();

  do
    {
      if (!irq)
        break;

      auto g = lock_guard(irq->irq_lock());
      irq->unbind();
    }
  while (!_del_observer.compare_exchange_weak(irq, nullptr));
}


// end of: IPC-gate deletion stuff -------------------------------
//
// state requests/manipulation
//

IMPLEMENT
void
Thread::do_leave_and_kill_myself()
{
  current_thread()->do_kill();
#ifdef CONFIG_JDB
  WARN("dead thread scheduled: %lx\n", current_thread()->dbg_id());
#endif
  kdb_ke("DEAD SCHED");
}

IMPLEMENT
Context::Drq::Result
Thread::handle_kill_helper(Drq *src, Context *, void *)
{
  Thread *to_delete = static_cast<Thread*>(static_cast<Kernel_drq*>(src)->src);
  assert (!to_delete->in_ready_list());
  if (to_delete->dec_ref() == 0)
    delete to_delete;

  return Drq::no_answer_resched();
}

IMPLEMENT
void
Thread::put_n_reap(Kobject ***reap_list)
{
  if (dec_ref() != 0)
    return;

  // we need to re-add the reference
  // that is released during Reap_list::del
  inc_ref();
  initiate_deletion(reap_list);
}

IMPLEMENT
bool
Thread::do_kill()
{
  //
  // Kill this thread
  //

  // But first prevent it from being woken up by asynchronous events

  {
    auto guard = lock_guard(cpu_lock);

    // if IPC timeout active, reset it
    reset_timeout();

    Sched_context::Ready_queue &rq = Sched_context::rq.current();

    // Switch to time-sharing scheduling context
    if (sched() != sched_context())
      switch_sched(sched_context(), &rq);

    if (!rq.current_sched() || rq.current_sched()->context() == this)
      rq.set_current_sched(current()->sched());
  }

  // if other threads want to send me IPC messages, abort these
  // operations
  {
    auto guard = lock_guard(cpu_lock);
    while (Sender *s = Sender::cast(sender_list()->first()))
      {
        s->sender_dequeue(sender_list());
        s->ipc_receiver_aborted();
        Proc::preemption_point();
      }
  }

  // if engaged in IPC operation, stop it
  if (in_sender_list())
    {
      while (Locked_prio_list *q = wait_queue())
        {
          auto g = lock_guard(q->lock());
          if (wait_queue() == q)
            {
              sender_dequeue(q);
              set_wait_queue(0);
              break;
            }
        }
    }

  if (utcb().kern())
    utcb().access()->free_marker = Utcb::Free_marker;
  // No UTCB access beyond this point!

  release_fpu_if_owner();

  vcpu_enter_kernel_mode(vcpu_state().access());
  vcpu_update_state();

  unbind();
  set_vcpu_user_space(nullptr);

  cpu_lock.lock();

  arch_vcpu_ext_shutdown();

  state.change_dirty(~Thread_dying, Thread_dead);

  // dequeue from system queues
  Sched_context::rq.current().ready_dequeue(sched());

  unregister_delete_irq();

  rcu_wait();

  state.del_dirty(Thread_ready_mask);

  Sched_context::rq.current().ready_dequeue(sched());

  // make sure this thread really never runs again by migrating it
  // to the 'invalid' CPU forcefully and then switching to the kernel
  // thread for doing the last bits.
  Sched<>::force_to_invalid_cpu(this);
  kernel_context_drq(handle_kill_helper, 0);
  kdb_ke("Im dead");
  return true;
}

IMPLEMENT
Context::Drq::Result
Thread::handle_remote_kill(Drq *, Context *self, void *)
{
  nonull_static_cast<Thread*>(self)->prepare_kill();
  return Drq::done();
}


IMPLEMENT
bool
Thread::kill()
{
  auto guard = lock_guard(cpu_lock);

  if (home_cpu() == current_cpu())
    {
      prepare_kill();
      Sched_context::rq.current().deblock(sched(), current()->sched());
      return true;
    }

  drq(Thread::handle_remote_kill, 0);

  return true;
}

IMPLEMENT
long
Thread::control(Thread_ptr const &pager, Thread_ptr const &exc_handler)
{
  if (pager.is_valid())
    _pager = pager;

  if (exc_handler.is_valid())
    _exc_handler = exc_handler;

  return 0;
}

PUBLIC
void
Thread::ipc_receiver_aborted() override
{
  assert (cpu_lock.test());
  assert (wait_queue());
  set_wait_queue(0);

  utcb().access()->error = L4_error::Canceled;

  if (xcpu_state_change(~0UL, Thread_transfer_failed | Thread_ready, true))
    current()->switch_to_locked(this);
}


//---------------------------------------------------------------------------
IMPLEMENTATION [!log]:

static inline
void Thread::page_fault_log(Address, unsigned, unsigned)
{}

PUBLIC static inline
int Thread::log_page_fault()
{ return 0; }

// ----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"
#include <sched.h>


IMPLEMENT
void
Thread::handle_global_remote_requests_irq()
{
  assert (cpu_lock.test());
  // printf("CPU[%2u]: > RQ IPI (current=%p)\n", current_cpu(), current());
  Ipi::eoi(Ipi::Global_request, current_cpu());
  Cpu_call::handle_global_requests();
}


//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"
#include "kdb_ke.h"
#include "terminate.h"

IMPLEMENT
void
Thread::system_abort()
{
  if (nested_trap_handler)
    kdb_ke("abort");

  terminate(EXIT_FAILURE);
}

IMPLEMENT
void
Thread::halt()
{
  // Cancel must be cleared on all kernel entry paths. See slowtraps for
  // why we delay doing it until here.
  state.del(Thread_cancel);

  // we haven't been re-initialized (cancel was not set) -- so sleep
  if (state.change_safely(~Thread_ready, Thread_cancel | Thread_dead))
    while (! (state() & Thread_ready))
      schedule();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

#include "terminate.h"

IMPLEMENT
void
Thread::system_abort()
{ terminate(EXIT_FAILURE); }
