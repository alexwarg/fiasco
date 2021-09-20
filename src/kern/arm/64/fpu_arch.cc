#include <fpu_arch.h>
#include <fpu.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "mem.h"
#include "processor.h"
#include "trap_state.h"
#include <globalconfig.h>

#ifdef CONFIG_LAZY_FPU

inline void finish_init(Fpu &f)
{
  f.disable();
  f.set_owner(0);
}

#else // CONFIG_LAZY_FPU

inline void finish_init(Fpu &)
{}

#endif // CONFIG_LAZY_FPU

void
Fpu_arch::init(Cpu_number cpu, bool resume)
{
  if (Config::Jdb && !resume && cpu == Cpu_number::boot_cpu())
    printf("FPU: Initialize\n");

  Fpu_arch_base::init(cpu, resume);

  Fpu &f = Fpu::fpu.cpu(cpu);

  // make sure that in HYP case CPACR is loaded and enabled.
  // without HYP the disable below will disable it, so this does not hurt
  __asm__ __volatile__ (
      "msr  CPACR_EL1, %[cpacr_on]"
      : : [cpacr_on]"r"(3UL << 20));

  finish_init(f);
}

