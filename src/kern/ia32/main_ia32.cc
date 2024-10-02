
#include "main.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "config.h"
#include "io.h"
#include "idt.h"
#include "kdb_ke.h"
#include "kernel_console.h"
#include "koptions.h"
#include <pc_i8259.h>
#include <pfc.h>
#include "processor.h"
#include "timer_tick.h"
#include "terminate.h"

static int exit_question_active;

extern "C" [[noreturn]] void
_exit(int)
{
  if (exit_question_active)
    Pfc::get()->system_reboot();

  while (1)
    {
      Proc::halt();
      Proc::pause();
    }

  __builtin_unreachable();
}

static
void
exit_question()
{
  Proc::cli();
  exit_question_active = 1;

  Unsigned16 irqs = Pc_i8259().disable_all_save();
  if (Config::getchar_does_hlt_works_ok)
    {
      Timer_tick::set_vectors_stop();
      Timer_tick::enable(Cpu_number::boot_cpu()); // hm, exit always on CPU 0
      Proc::sti();
    }

  // make sure that we don't acknowledge the exit question automatically
  Kconsole::console()->change_state(Console::PUSH, 0, ~Console::INENABLED, 0);
  puts("\nReturn reboots, \"k\" enters L4 kernel debugger...");

  char c = Kconsole::console()->getchar();

  if (c == 'k' || c == 'K') 
    {
      Pc_i8259().restore_all(irqs);
      kdb_ke("_exit");
    }
  else
    {
      // It may be better to not call all the destruction stuff because of
      // unresolved static destructor dependency problems. So just do the
      // reset at this point.
      puts("\033[1mRebooting.\033[m");
    }
}

void main_arch();

FIASCO_INIT void
main_arch()
{
  // console initialization
  set_exit_question(&exit_question);
}

