
#pragma once

#include <globalconfig.h>
#include <types.h>

#ifdef CONFIG_JDB

#include <dbg_stack.h>
#include <thread.h>
#include <cpu_call.h>
#include <ipi.h>
#include <debug_traps.h>

/** Call the nested trap handler (either Jdb::enter_kdebugger() or the
 * gdb stub. Setup our own stack frame */
static int
call_nested_trap_handler(Trap_state *ts)
{
  Proc::cli();

  Cpu_number log_cpu = dbg_find_cpu();
  unsigned long &ntr = Thread::nested_trap_recover.cpu(log_cpu);

#if 0
  printf("%s: lcpu%u sp=%p t=%u nested_trap_recover=%ld\n",
      __func__, log_cpu, (void*)Proc::stack_pointer(), ts->_trapno,
      ntr);
#endif

  Unsigned64 ret;
  void *stack = 0;
  if (!ntr)
    stack = Dbg::dbg_stack.cpu(log_cpu).stack_top;

  Unsigned64 dummy1, dummy2, scratch1, scratch2;

  // don't set %esp if gdb fault recovery to ensure that exceptions inside
  // kdb/jdb don't overwrite the stack
  asm volatile
    ("mov    %%rsp,%[d2]	\n\t"	// save old stack pointer
     "cmpq   $0,%[recover]	\n\t"
     "jne    1f			\n\t"	// check trap within trap handler
     "mov    %[stack],%%rsp	\n\t"	// setup clean stack pointer
     "1:			\n\t"
     "incq   %[recover]		\n\t"
#ifndef CONFIG_CPU_LOCAL_MAP
     "mov    %%cr3, %[d1]	\n\t"
#endif
     "push   %[d2]		\n\t"	// save old stack pointer on new stack
     "push   %[d1]		\n\t"	// save old pdbr
#ifndef CONFIG_CPU_LOCAL_MAP
     "mov    %[pdbr], %%cr3	\n\t"
#endif
     "callq  *%[handler]	\n\t"
     "pop    %[d1]		\n\t"
#ifndef CONFIG_CPU_LOCAL_MAP
     "mov    %[d1], %%cr3	\n\t"
#endif
     "pop    %%rsp		\n\t"	// restore old stack pointer
     "cmpq   $0,%[recover]	\n\t"	// check trap within trap handler
     "je     1f			\n\t"
     "decq   %[recover]		\n\t"
     "1:			\n\t"
     : [ret] "=&a"(ret), [d2] "=&d"(dummy2), [d1] "=&c"(dummy1), "=D"(scratch1),
       "=S"(scratch2),
       [recover] "+m" (ntr)
     : [ts] "D" (ts),
#ifndef CONFIG_CPU_LOCAL_MAP
       [pdbr] "r" (Kernel_task::kernel_task()->virt_to_phys((Address)Kmem::dir())),
#endif
       [cpu] "S" (log_cpu),
       [stack] "r" (stack),
       [handler] "m" (Thread::nested_trap_handler)
     : "r8", "r9", "r10", "r11", "memory");

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret == 0 ? 0 : -1;
}
#endif // CONFIG_JDB

class Trap_state;
inline int
check_trap13_kernel(Trap_state * /*ts*/)
{ return 1; }

