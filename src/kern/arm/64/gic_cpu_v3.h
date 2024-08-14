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
    asm volatile("msr S3_4_C12_C9_5, %x0" // ICC_SRE_EL2
                 : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
#endif // CONFIG_CPU_VIRT
    asm volatile("msr S3_0_C12_C12_5, %x0" // ICC_SRE_EL1
                 : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
  }

public:
  void pmr(unsigned prio)
  {
    asm volatile("msr S3_0_C4_C6_0, %x0" : : "r" (prio)); // ICC_PMR_EL1
  }

  void enable()
  {
    _enable_sre_set();
    Mem::isb();

    asm volatile("msr S3_0_C12_C12_7, %x0" : : "r" (1ul)); // ICC_IGRPEN1_EL1

    pmr(Cpu_prio_val);
  }

  void disable()
  {
    asm volatile("msr S3_0_C12_C12_7, %x0" : : "r" (0ul)); // ICC_IGRPEN1_EL1
    Mem::isb();
  }

  void ack(Unsigned32 irq)
  {
    asm volatile("msr S3_0_C12_C12_1, %x0" : : "r"(irq)); // ICC_EOIR1_EL1
    Mem::isb();
  }

  Unsigned32 iar()
  {
    Unsigned32 v;
    asm volatile("mrs %x0, S3_0_C12_C12_0" : "=r"(v)); // ICC_IAR1_EL1
    Mem::dsb();
    return v;
  }

  unsigned pmr()
  {
    Unsigned32 pmr;
    asm volatile("mrs %x0, S3_0_C4_C6_0" : "=r"(pmr)); // ICC_PMR_EL1
    return pmr;
  }

  void softint(Unsigned64 sgi)
  {
    asm volatile("msr S3_0_C12_C11_5, %x0" // ICC_SGI1R_EL1
                 : : "r"(sgi));
  }
};
