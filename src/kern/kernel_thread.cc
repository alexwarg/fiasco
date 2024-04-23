
#include <kernel_thread.h>
#include <per_cpu_data_alloc.h>
#include <delayloop.h>
#include <system_clock.h>
#include <config.h>
#include <cpu.h>
#include <globals.h>
#include <helping_lock.h>
#include <kernel_task.h>
#include <processor.h>
#include <task.h>
#include <thread.h>
#include <thread_state.h>
#include <timer_tick.h>
#include <watchdog.h>

#include <globalconfig.h>

/**
 * unit test interface
 */
[[gnu::weak]] void init_unittest();

#ifndef CONFIG_MP
void
Kernel_thread::boot_app_cpus()
{}
#endif

#ifdef CONFIG_JDB

#include <koptions.h>

static void check_debug_koptions()
{
  auto g = lock_guard(cpu_lock);

  if (Config::Jdb &&
      !Koptions::o()->opt(Koptions::F_nojdb) &&
      Koptions::o()->opt(Koptions::F_jdb_cmd))
    {
      // extract the control sequence from the command line
      String_buf<128> cmd;
      for (char const *s = Koptions::o()->jdb_cmd; *s && *s != ' '; ++s)
        cmd.append(*s);

      kdb_ke_sequence(cmd.c_str(), cmd.length());
    }

  // kernel debugger rendezvous
  if (Koptions::o()->opt(Koptions::F_wait))
    kdb_ke("Wait");
}
#else
static void check_debug_koptions()
{}
#endif

struct Cpu_idle_generic : Cpu_idle_iface
{
  void idle() override
  {
    if (Config::hlt_works_ok)
      Proc::halt();			// stop the CPU, waiting for an int
    else
      Proc::pause();
  }
};


static Cpu_idle_generic __generic_idle;
Cpu_idle_iface *Kernel_thread::idle = &__generic_idle;


// the kernel bootstrap routine
FIASCO_INIT
void
Kernel_thread::bootstrap()
{
  // Initializations done -- Helping_lock can now use helping lock
  Helping_lock::threading_system_active = true;

  // we need per CPU data for our never running dummy CPU too
  // FIXME: we in fact need only the _pending_rqq lock
  Per_cpu_data_alloc::alloc(Cpu::invalid());
  Per_cpu_data::run_ctors(Cpu::invalid());
  set_current_cpu(Cpu::boot_cpu()->id());
  _home_cpu = Cpu::boot_cpu()->id();
  Mem::barrier();

  state.change_dirty(0, Thread_ready);		// Set myself ready

  System_clock::init();
  Sched_context::rq.current().set_idle(this->sched());

  Kernel_task::kernel_task()->make_current();

  // Setup initial timeslice
  Sched_context::rq.current().set_current_sched(sched());

  Timer_tick::setup(current_cpu());
  assert (current_cpu() == Cpu_number::boot_cpu()); // currently the boot cpu must be 0
  Mem_space::enable_tlb(current_cpu());

  Per_cpu_data::run_late_ctors(Cpu_number::boot_cpu());
  Per_cpu_data::run_late_ctors(Cpu::invalid());
  bootstrap_arch();

  // Needs to be done before the timer is enabled. Otherwise after returning
  // from printf() there could be a burst of timer interrupts distorting the
  // timer loop calibration. The measurement intervals would be far too short.
  printf("Calibrating timer loop... ");
  Timer_tick::enable(current_cpu());
  Proc::sti();
  Watchdog::enable();
  // Init delay loop, needs working timer interrupt
  Delay::init();
  printf("done.\n");

  run();
}

/**
 * The idle loop
 * NEVER inline this function, because our caller is an initcall
 */
FIASCO_NOINLINE FIASCO_NORETURN
void
Kernel_thread::run()
{
  clean_initcall_section();

  // No initcalls after this point!

  kernel_context(home_cpu(), this);

  Rcu::leave_idle(home_cpu());

  check_debug_koptions();

  // init_workload cannot be an initcall, because it fires up the userland
  // applications which then have access to initcall frames as per kinfo page.
  if (init_unittest)
    init_unittest();
  else
    init_workload();

  for (;;)
    idle->idle();
}


