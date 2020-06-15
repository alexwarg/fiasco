
#include <perf_cnt_arm_v7plus_bits.h>
#include <cpu.h>
#include <std_macros.h>

void Perf_cnt_arm_v7plus_64::ccnt_init(Cpu const &cpu)
{
  if (cpu.has_pmuv3())
    {
      Mword val;
      asm volatile ("mrs %0, PMCCFILTR_EL0" : "=r" (val));
      val &= ~(1UL << 31); //   P=0: don't disable counting of cycles in EL1
      val &= ~(1UL << 30); //   U=0: don't disable counting of cycles in EL0
      val &= ~(1UL << 29); // NSK=0: don't disable counting of cycles in
                           // non-secure EL1
      val &= ~(1UL << 28); // NSU=0: don't disable counting of cycles in
                           // non-secure EL0
      if (IS_ENABLED(CONFIG_CPU_VIRT))
        val |= (1UL << 27); // NSH=1: don't disable counting of cycles in EL2
      asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (val));
    }
}

