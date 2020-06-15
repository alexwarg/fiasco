
#include <perf_cnt_arm_v7plus_bits.h>
#include <cpu.h>
#include <std_macros.h>

void Perf_cnt_arm_v7plus_32::ccnt_init(Cpu const &cpu)
{
  // Make sure that the cycle counters also count cycles in EL2.
  if (cpu.has_pmuv2())
    {
      pmnxsel(0x1f);
      Mem::isb();

      // PMXEVTYPER now alias for PMCCFILTR
      Mword val;
      asm volatile ("mrc p15, 0, %0, c9, c13, 1" : "=r" (val)); // PMXEVTYPER
      val &= ~(1UL << 31); //   P=0: don't disable counting of cycles in EL1
      val &= ~(1UL << 30); //   U=0: don't disable counting of cycles in EL0
      val &= ~(1UL << 29); // NSK=0: don't disable counting of cycles in
                           // non-secure EL1
      val &= ~(1UL << 28); // NSU=0: don't disable counting of cycles in
                           // non-secure EL0
      if (IS_ENABLED(CONFIG_CPU_VIRT))
        val |= (1UL << 27); // NSH=1: count cycles in EL2
      asm volatile ("mcr p15, 0, %0, c9, c13, 1" :: "r" (val)); // PMXEVTYPER
    }
}
