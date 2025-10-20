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
  Cpu_number log_cpu = dbg_find_cpu();
  unsigned long &ntr = Thread::nested_trap_recover.cpu(log_cpu);

#if 0
  printf("%s: lcpu%u sp=%p t=%lu nested_trap_recover=%ld\n",
         __func__, log_cpu, (void*)Proc::stack_pointer(), ts->_trapno, ntr);
#endif

  int ret;

  unsigned dummy1, dummy2, dummy3;

  struct
  {
    Mword pdir;
    FIASCO_FASTCALL int (*handler)(Trap_state*, Cpu_number);
    void *stack;
  } p;

  if (!ntr)
    p.stack = Dbg::dbg_stack.cpu(log_cpu).stack_top;
  else
    p.stack = 0;

  p.pdir = Kernel_task::kernel_task()->virt_to_phys((Address)Kmem::dir());
  p.handler = Thread::nested_trap_handler;

  // don't set %esp if gdb fault recovery to ensure that exceptions inside
  // kdb/jdb don't overwrite the stack
  asm volatile
    ("mov    %%esp,%[d2]	\n\t"
     "cmpl   $0,(%[ntr])	\n\t"
     "jne    1f			\n\t"
     "mov    8(%[p]),%%esp	\n\t"
     "1:			\n\t"
     "incl   (%[ntr])		\n\t"
     "mov    %%cr3, %[d1]	\n\t"
     "push   %[d2]		\n\t"
     "push   %[p]		\n\t"
     "push   %[d1]		\n\t"
     "mov    (%[p]), %[d1]	\n\t"
     "mov    %[d1], %%cr3	\n\t"
     "call   *4(%[p])		\n\t"
     "pop    %[d1]		\n\t"
     "mov    %[d1], %%cr3	\n\t"
     "pop    %[p]		\n\t"
     "pop    %%esp		\n\t"
     "cmpl   $0,(%[ntr])	\n\t"
     "je     1f			\n\t"
     "decl   (%[ntr])		\n\t"
     "1:			\n\t"

     : [ret] "=a"  (ret),
       [d1]  "=&c" (dummy1),
       [d2]  "=&r" (dummy2),
             "=d"  (dummy3)
     : [ts]      "a" (ts),
       [cpu]     "d" (log_cpu),
       [p]       "S" (&p),
       [ntr]     "D" (&ntr)
     : "memory");

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret == 0 ? 0 : -1;
}

#endif // CONFIG_JDB


inline int
check_trap13_kernel(Trap_state *ts)
{
  if (EXPECT_FALSE(ts->_trapno == 13 && (ts->_err & 3) == 0))
    {
      // First check if user loaded a segment register with 0 because the
      // resulting exception #13 can be raised from user _and_ kernel. If
      // the user tried to load another segment selector, the thread gets
      // killed.
      // XXX Should we emulate this too? Michael Hohmuth: Yes, we should.
      if (EXPECT_FALSE(!(ts->_ds & 0xffff)))
	{
	  Cpu::set_ds(Gdt::data_segment());
	  return 0;
	}
      if (EXPECT_FALSE(!(ts->_es & 0xffff)))
	{
	  Cpu::set_es(Gdt::data_segment());
	  return 0;
	}
      if (EXPECT_FALSE(ts->_ds & 0xfff8) == Gdt::gdt_code_user)
	{
	  WARN("%p eip=%08lx: code selector ds=%04lx\n",
               current(), ts->ip(), ts->_ds & 0xffff);
	  Cpu::set_ds(Gdt::data_segment());
	  return 0;
	}
      if (EXPECT_FALSE(ts->_es & 0xfff8) == Gdt::gdt_code_user)
	{
	  WARN("%p eip=%08lx: code selector es=%04lx\n",
               current(), ts->ip(), ts->_es & 0xffff);
	  Cpu::set_es(Gdt::data_segment());
	  return 0;
	}
      if (EXPECT_FALSE(ts->_fs & 0xfff8) == Gdt::gdt_code_user)
	{
	  WARN("%p eip=%08lx: code selector fs=%04lx\n",
               current(), ts->ip(), ts->_fs & 0xffff);
	  ts->_fs = 0;
	  return 0;
	}
      if (EXPECT_FALSE(ts->_gs & 0xfff8) == Gdt::gdt_code_user)
	{
	  WARN("%p eip=%08lx: code selector gs=%04lx\n",
               current(), ts->ip(), ts->_gs & 0xffff);
	  ts->_gs = 0;
	  return 0;
	}
    }

  return 1;
}


