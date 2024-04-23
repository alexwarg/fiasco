#include <kernel_thread.h>
#include <config.h>

FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  Proc::sti();
  boot_app_cpus();
  Proc::cli();
}

