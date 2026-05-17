#include <kernel_thread.h>
#include <config.h>
#include <pfc.h>

FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  Proc::sti();
  Pfc::get()->boot_ap_cpus();
  Proc::cli();
}

