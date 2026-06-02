
#include <config.h>


#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include "feature.h"
#include "initcalls.h"
#include "koptions.h"
#include "panic.h"
#include "std_macros.h"

KIP_KERNEL_ABI_VERSION(FIASCO_STRINGIFY(FIASCO_KERNEL_SUBVERSION));

// class variables
bool Config::esc_hack = false;
#ifdef CONFIG_SERIAL
int  Config::serial_esc = Config::SERIAL_NO_ESC;
#endif

unsigned Config::tbuf_entries = 0x20000 / sizeof(Mword); //1024;
bool Config::getchar_does_hlt_works_ok = false;

#ifdef CONFIG_FINE_GRAINED_CPUTIME
KIP_KERNEL_FEATURE("fi_gr_cputime");
#endif

FIASCO_INIT
void Config::init()
{
  init_arch();

  if (Koptions::o()->opt(Koptions::F_esc))
    esc_hack = true;

#ifdef CONFIG_SERIAL
  if (    Koptions::o()->opt(Koptions::F_serial_esc)
      && !Koptions::o()->opt(Koptions::F_noserial)
      && !Koptions::o()->opt(Koptions::F_nojdb))
    {
      serial_esc = SERIAL_ESC_IRQ;
    }
#endif
}

FIASCO_INIT
unsigned long
Config::kmem_size(unsigned long available_size)
{
#ifdef CONFIG_KMEM_SIZE_AUTO
  static_assert(kmem_per_cent() < 100, "Sanitize kmem_per_cent");
  unsigned long alloc_size = available_size / 100U * kmem_per_cent();
  if (alloc_size > kmem_max())
    alloc_size = kmem_max();
  return alloc_size;
#else
  (void)available_size;
  return static_cast<unsigned long>(CONFIG_KMEM_SIZE_KB) << 10;
#endif
}

