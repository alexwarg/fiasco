#include <thread.h>

#include <cassert>
#include <cstdlib>		// panic()
#include <cstring>
#include <cxx/atomic>
#include <entry_frame.h>
#include <globals.h>
#include <irq_chip.h>
#include <kdb_ke.h>
#include <kernel_task.h>
#include <kmem_alloc.h>
#include <logdefs.h>
#include <sched_context.h>
#include <space.h>
#include <std_macros.h>
#include <task.h>
#include <thread_state.h>
#include <timeout.h>

JDB_DEFINE_TYPENAME(Thread,  "\033[32mThread\033[m");
DEFINE_PER_CPU Per_cpu<unsigned long> Thread::nested_trap_recover;

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
namespace {
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
} // namspace

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

void
Thread::do_leave_and_kill_myself()
{
  current_thread()->do_kill();
#ifdef CONFIG_JDB
  WARN("dead thread scheduled: %lx\n", current_thread()->dbg_id());
#endif
  kdb_ke("DEAD SCHED");
}

Context::Drq::Result
Thread::handle_kill_helper(Drq *src, Context *, void *)
{
  Thread *to_delete = static_cast<Thread*>(static_cast<Kernel_drq*>(src)->src);
  assert (!to_delete->in_ready_list());
  if (to_delete->dec_ref() == 0)
    delete to_delete;

  return Drq::no_answer_resched();
}

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

Context::Drq::Result
Thread::handle_remote_kill(Drq *, Context *self, void *)
{
  nonull_static_cast<Thread*>(self)->prepare_kill();
  return Drq::done();
}


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

long
Thread::control(Thread_ptr const &pager, Thread_ptr const &exc_handler)
{
  if (pager.is_valid())
    _pager = pager;

  if (exc_handler.is_valid())
    _exc_handler = exc_handler;

  return 0;
}

void
Thread::ipc_receiver_aborted()
{
  assert (cpu_lock.test());
  assert (wait_queue());
  set_wait_queue(0);

  utcb().access()->error = L4_error::Canceled;

  if (xcpu_state_change(~0UL, Thread_transfer_failed | Thread_ready, true))
    current()->switch_to_locked(this);
}


#ifdef CONFIG_MP

#include <ipi.h>
#include <sched.h>

void
Thread::handle_global_remote_requests_irq()
{
  assert (cpu_lock.test());
  // printf("CPU[%2u]: > RQ IPI (current=%p)\n", current_cpu(), current());
  Ipi::eoi(Ipi::Global_request, current_cpu());
  Cpu_call::handle_global_requests();
}

#endif // CONFIG_MP

#ifdef CONFIG_JDB

#include <string_buffer.h>
#include <kdb_ke.h>
#include <terminate.h>

void
Thread::system_abort()
{
  if (nested_trap_handler)
    kdb_ke("abort");

  terminate(EXIT_FAILURE);
}

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

#else // CONFIG_JDB

#include <terminate.h>

void
Thread::system_abort()
{ terminate(EXIT_FAILURE); }

#endif // CONFIG_JDB
