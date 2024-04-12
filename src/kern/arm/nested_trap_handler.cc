
#include <nested_trap_handler.h>
#include <trap_state.h>
#include <globalconfig.h>

#ifdef CONFIG_JDB

#include <arm_enter_debugger.h>
#include <kernel_task.h>
#include <mem_layout.h>
#include <mmu.h>

#include <dbg_stack.h>
#include <std_macros.h>

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

inline bool
debugger_needs_switch_to_kdir()
{ return !(IS_ENABLED(CONFIG_BIT64) || IS_ENABLED(CONFIG_CPU_VIRT)); }


int
call_nested_trap_handler(Trap_state *ts)
{
  Cpu_phys_id phys_cpu = Proc::cpu_id();
  Cpu_number log_cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(phys_cpu));
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n",
             cxx::int_value<Cpu_phys_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }

  unsigned long &ntr = Thread::nested_trap_recover.cpu(log_cpu);

  void *stack = 0;

  if (!ntr)
    stack = Dbg::dbg_stack.cpu(log_cpu).stack_top;

  Mem_space *m = Mem_space::current_mem_space(log_cpu);

  if (debugger_needs_switch_to_kdir() && (Kernel_task::kernel_task() != m))
    Kernel_task::kernel_task()->make_current();

  int ret = arm_enter_debugger(ts, log_cpu, &ntr, stack);

  // the jdb-cpu might have changed things we shouldn't miss!
  Mmu<Mem_layout::Cache_flush_area, true>::flush_cache();
  Mem::isb();

  if (debugger_needs_switch_to_kdir() && (m != Kernel_task::kernel_task()))
    m->make_current();

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

#else // CONFIG_JDB

#include <thread.h>

int call_nested_trap_handler(Trap_state *ts)
{
  ts->dump();
  current_thread()->kill();
  return -1;
}

#endif // CONFIG_JDB
