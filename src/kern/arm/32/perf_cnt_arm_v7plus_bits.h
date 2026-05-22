
#pragma once

#include "types.h"

// ARM32 PMUv1/v2 register access via CP15 coprocessor instructions.
namespace Perf_cnt_arm_v7plus_32
{
  inline void pmcr(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (val)); }

  inline Mword pmcr()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (val)); return val; }

  inline void cntens(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (val)); }

  inline Mword cntens()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 1" : "=r" (val)); return val; }

  inline void cntenc(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 2" : : "r" (val)); }

  inline Mword cntenc()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 2" : "=r" (val)); return val; }

  inline void flag(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 3" : : "r" (val)); }

  inline Mword flag()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 3" : "=r" (val)); return val; }

  inline void pmnxsel(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 5" : : "r" (val)); }

  inline Mword pmnxsel()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 5" : "=r" (val)); return val; }

  inline void ccnt(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 0" : : "r" (val)); }

  inline Mword ccnt()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (val)); return val; }

  inline void evtsel(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 1" : : "r" (val)); }

  inline Mword evtsel()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 1" : "=r" (val)); return val; }

  inline void pmcnt(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 2" : : "r" (val)); }

  inline Mword pmcnt()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 2" : "=r" (val)); return val; }

  inline void useren(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 0" : : "r" (val)); }

  inline Mword useren()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 0" : "=r" (val)); return val; }

  inline void intens(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 1" : : "r" (val)); }

  inline Mword intens()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 1" : "=r" (val)); return val; }

  inline void intenc(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 2" : : "r" (val)); }

  inline Mword intenc()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 2" : "=r" (val)); return val; }
};

namespace Perf_cnt_arm_v7plus_bits {
using namespace Perf_cnt_arm_v7plus_32;
}

