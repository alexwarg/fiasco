#pragma once

#include "gic_cpu_v3_generic.h"
#include "mem_unit.h"
#include "globalconfig.h"
#include <mem.h>

class Gic_cpu_v3 : public Gic_cpu_v3_generic
{
  void _enable_sre_set()
  {
#if defined (CONFIG_CPU_VIRT)
    asm volatile("mcr p15, 4, %0, c12, c9, 5" // ICC_HSRE
                 : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
#endif // CONFIG_CPU_VIRT
    asm volatile("mcr p15, 0, %0, c12, c12, 5" // ICC_SRE
                 : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
  }

public:
  void pmr(unsigned prio)
  {
    asm volatile("mcr p15, 0, %0, c4, c6, 0" : : "r"(prio));
  }

  void enable()
  {
    _enable_sre_set();

    asm volatile("mcr p15, 0, %0, c12, c12, 7" : : "r" (1)); // ICC_IGRPEN1

    pmr(Cpu_prio_val);
  }

  void disable()
  {
    asm volatile("mcr p15, 0, %0, c12, c12, 7" : : "r" (0)); // ICC_IGRPEN1
    Mem::isb();
  }

  void ack(Unsigned32 irq)
  {
    asm volatile("mcr p15, 0, %0, c12, c12, 1" : : "r"(irq));
  }

  Unsigned32 iar()
  {
    Unsigned32 v;
    asm volatile("mrc p15, 0, %0, c12, c12, 0" : "=r"(v));
    return v;
  }

  unsigned pmr()
  {
    Unsigned32 pmr;
    asm volatile("mrc p15, 0, %0, c4, c6, 0" : "=r"(pmr));
    return pmr;
  }

  void softint(Unsigned64 sgi)
  {
    asm volatile("mcrr p15, 0, %Q0, %R0, c12"
                 : : "r"(sgi));
  }
};
