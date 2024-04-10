
#include "app_cpu_thread.h"

#include <cstdlib>
#include <cstdio>

#include "config.h"
#include "fpu.h"
#include "globals.h"
#include "helping_lock.h"
#include "kernel_task.h"
#include "processor.h"
#include "rcupdate.h"
#include "scheduler_iface.h"
#include "task.h"
#include "thread.h"
#include "thread_state.h"
#include "timer_tick.h"
#include "spin_lock.h"
#include "warn.h"

/**
 * unit test interface for app cores.
 */
extern void init_unittest_app_core() __attribute__((weak));


Kernel_thread *
App_cpu_thread::may_be_create(Cpu_number cpu, bool cpu_never_seen_before)
{
  if (!cpu_never_seen_before)
    {
      kernel_context(cpu)->reset_kernel_sp();
      return static_cast<Kernel_thread *>(kernel_context(cpu));
    }

  Kernel_thread *t = new (Ram_quota::root) App_cpu_thread(Ram_quota::root);
  assert (t);

  t->set_home_cpu(cpu);
  t->set_current_cpu(cpu);
  t->kbind(Kernel_task::kernel_task());
  return t;
}


// the kernel bootstrap routine
void
App_cpu_thread::bootstrap(Mword resume)
{
  extern Spin_lock<Mword> _tramp_mp_spinlock;

  state.change_dirty(0, Thread_ready);		// Set myself ready
  auto ccpu = current_cpu();

  Fpu::init(ccpu, resume);

  // initialize the current_mem_space function to point to the kernel space
  Kernel_task::kernel_task()->make_current();

  Mem_unit::tlb_flush();

  Cpu::cpus.current().set_present(1);
    {
      auto guard = lock_guard(_pending_rqq.current().q_lock());
      Cpu::cpus.current().set_online(1);
    }

  _tramp_mp_spinlock.set(1);

  if (!resume)
    {
      kernel_context(ccpu, this);
      Sched_context::rq.current().set_idle(this->sched());

      Timer_tick::setup(ccpu);
    }

  Rcu::leave_idle(ccpu);
  Mem_space::enable_tlb(ccpu);

  if (!resume)
    // Setup initial timeslice
    Sched_context::rq.current().set_current_sched(sched());

  if (!resume)
    Per_cpu_data::run_late_ctors(ccpu);

  Scheduler_iface::root()->trigger_hotplug_event();
  Timer_tick::enable(ccpu);
  cpu_lock.clear();

  if (!resume)
    {
      Cpu::cpus.current().print_infos();
      if (Warn::is_enabled(Info))
        printf("CPU[%u]: goes to idle loop\n", cxx::int_value<Cpu_number>(ccpu));
    }

  if (init_unittest_app_core)
    init_unittest_app_core();

  for (;;)
    idle_op();
}

