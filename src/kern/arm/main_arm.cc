
#include "main.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <entry.h>

#include "config.h"
#include "globals.h"
#include "initcalls.h"
#include "kmem_alloc.h"
#include "kip_init.h"
#include "kdb_ke.h"
#include "kernel_thread.h"
#include "kernel_task.h"
#include "kernel_console.h"
#include <pfc.h>
#include "space.h"
#include "terminate.h"

#include "processor.h"

static int exit_question_active = 0;

extern "C" void __attribute__ ((noreturn))
_exit(int)
{
  if (exit_question_active && Pfc::get())
    Pfc::get()->system_reboot();

  while (1)
    {
      Proc::halt();
      Proc::pause();
    }
}


static void exit_question()
{
  exit_question_active = 1;

  while (1)
    {
      puts("\nReturn reboots, \"k\" enters L4 kernel debugger...");

      char c = Kconsole::console()->getchar();

      if (c == 'k' || c == 'K')
	{
	  kdb_ke("_exit");
	}
      else
	{
	  // it may be better to not call all the destruction stuff
	  // because of unresolved static destructor dependency
	  // problems.
	  // SO just do the reset at this point.
	  puts("\033[1mRebooting...\033[0m");
          Pfc::get()->system_reboot();
	  break;
	}
    }
}

void
kernel_main()
{
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  // make some basic initializations, then create and run the kernel
  // thread
  set_exit_question(&exit_question);

  printf("%s\n", Kip::k()->version_string());

  // disallow all interrupts before we selectively enable them
  //  pic_disable_all();

  // create kernel thread
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread(Ram_quota::root);
  Task *const ktask = Kernel_task::kernel_task();
  kernel->kbind(ktask);
  assert(((Mword)kernel->init_stack() & 7) == 0);

  Mem_unit::tlb_flush();

  extern char call_bootstrap[];
  // switch to stack of kernel thread and bootstrap the kernel
  Entry::arm_fast_exit(kernel->init_stack(), call_bootstrap, kernel);

  // never returns here
}


