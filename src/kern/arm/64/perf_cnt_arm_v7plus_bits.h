#pragma once

#include "types.h"

class Cpu;

// ARM64 PMUv3 register access via MRS/MSR system register instructions.
namespace Perf_cnt_arm_v7plus_64
{
  inline void pmcr(Mword val)
  { asm volatile ("msr PMCR_EL0, %0" : : "r" (val)); }

  inline Mword pmcr()
  { Mword val; asm volatile ("mrs %0, PMCR_EL0" : "=r" (val)); return val; }

  inline void cntens(Mword val)
  { asm volatile ("msr PMCNTENSET_EL0, %0" : : "r" (val)); }

  inline Mword cntens()
  { Mword val; asm volatile ("mrs %0, PMCNTENSET_EL0" : "=r" (val)); return val; }

  inline void cntenc(Mword val)
  { asm volatile ("msr PMCNTENCLR_EL0, %0" : : "r" (val)); }

  inline void flag(Mword val)
  { asm volatile ("msr PMOVSCLR_EL0, %0" : : "r" (val)); }

  inline Mword flag()
  { Mword val; asm volatile ("mrs %0, PMOVSCLR_EL0" : "=r" (val)); return val; }

  inline void pmnxsel(Mword val)
  { asm volatile ("msr PMSELR_EL0, %0" : : "r" (val)); }

  inline Mword pmnxsel()
  { Mword val; asm volatile ("mrs %0, PMSELR_EL0" : "=r" (val)); return val; }

  inline void ccnt(Mword val)
  { asm volatile ("msr PMCCNTR_EL0, %0" : : "r" (val)); }

  inline Mword ccnt()
  { Mword val; asm volatile ("mrs %0, PMCCNTR_EL0" : "=r" (val)); return val; }

  void ccnt_init(Cpu const &cpu);

  inline void evtsel(Mword val)
  { asm volatile ("msr PMXEVTYPER_EL0, %0" : : "r" (val)); }

  inline Mword evtsel()
  { Mword val; asm volatile ("mrs %0, PMXEVTYPER_EL0" : "=r" (val)); return val; }

  inline void pmcnt(Mword val)
  { asm volatile ("msr PMXEVCNTR_EL0, %0" : : "r" (val)); }

  inline Mword pmcnt()
  { Mword val; asm volatile ("mrs %0, PMXEVCNTR_EL0" : "=r" (val)); return val; }

  inline void useren(Mword val)
  { asm volatile ("msr PMUSERENR_EL0, %0" : : "r" (val)); }

  inline Mword useren()
  { Mword val; asm volatile ("mrs %0, PMUSERENR_EL0" : "=r" (val)); return val; }

  inline void intens(Mword val)
  { asm volatile ("msr PMINTENSET_EL1, %0" : : "r" (val)); }

  inline Mword intens()
  { Mword val; asm volatile ("mrs %0, PMINTENSET_EL1" : "=r" (val)); return val; }

  inline void intenc(Mword val)
  { asm volatile ("msr PMINTENCLR_EL1, %0" : : "r" (val)); }
}

namespace Perf_cnt_arm_v7plus_bits {
using namespace Perf_cnt_arm_v7plus_64;
}

