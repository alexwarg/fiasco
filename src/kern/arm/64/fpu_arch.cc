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

  Fpu &f = Fpu::fpu.cpu(cpu);

  // make sure that in HYP case CPACR is loaded and enabled.
  // without HYP the disable below will disable it, so this does not hurt
  __asm__ __volatile__ (
      "msr  CPACR_EL1, %[cpacr_on]"
      : : [cpacr_on]"r"(3UL << 20));

  finish_init(f);
}

inline void
save_fpu_regs(Fpu_arch::Fpu_regs *r)
{
  Mword fpcr;
  Mword fpsr;
  asm volatile("stp     q0, q1,   [%[s], #16 *  0]        \n"
               "stp     q2, q3,   [%[s], #16 *  2]        \n"
               "stp     q4, q5,   [%[s], #16 *  4]        \n"
               "stp     q6, q7,   [%[s], #16 *  6]        \n"
               "stp     q8, q9,   [%[s], #16 *  8]        \n"
               "stp     q10, q11, [%[s], #16 * 10]        \n"
               "stp     q12, q13, [%[s], #16 * 12]        \n"
               "stp     q14, q15, [%[s], #16 * 14]        \n"
               "stp     q16, q17, [%[s], #16 * 16]        \n"
               "stp     q18, q19, [%[s], #16 * 18]        \n"
               "stp     q20, q21, [%[s], #16 * 20]        \n"
               "stp     q22, q23, [%[s], #16 * 22]        \n"
               "stp     q24, q25, [%[s], #16 * 24]        \n"
               "stp     q26, q27, [%[s], #16 * 26]        \n"
               "stp     q28, q29, [%[s], #16 * 28]        \n"
               "stp     q30, q31, [%[s], #16 * 30]        \n"
               "mrs     %[fpcr], fpcr                     \n"
               "mrs     %[fpsr], fpsr                     \n"
               : [fpcr] "=r" (fpcr),
                 [fpsr] "=r" (fpsr),
                 "=m" (r->state)
               : [s] "r" (r->state));
  r->fpcr = fpcr;
  r->fpsr = fpsr;
}

inline void
restore_fpu_regs(Fpu_arch::Fpu_regs const *r)
{
  asm volatile("ldp     q0, q1,   [%[s], #16 *  0]        \n"
               "ldp     q2, q3,   [%[s], #16 *  2]        \n"
               "ldp     q4, q5,   [%[s], #16 *  4]        \n"
               "ldp     q6, q7,   [%[s], #16 *  6]        \n"
               "ldp     q8, q9,   [%[s], #16 *  8]        \n"
               "ldp     q10, q11, [%[s], #16 * 10]        \n"
               "ldp     q12, q13, [%[s], #16 * 12]        \n"
               "ldp     q14, q15, [%[s], #16 * 14]        \n"
               "ldp     q16, q17, [%[s], #16 * 16]        \n"
               "ldp     q18, q19, [%[s], #16 * 18]        \n"
               "ldp     q20, q21, [%[s], #16 * 20]        \n"
               "ldp     q22, q23, [%[s], #16 * 22]        \n"
               "ldp     q24, q25, [%[s], #16 * 24]        \n"
               "ldp     q26, q27, [%[s], #16 * 26]        \n"
               "ldp     q28, q29, [%[s], #16 * 28]        \n"
               "ldp     q30, q31, [%[s], #16 * 30]        \n"
               "msr     fpcr, %[fpcr]                     \n"
               "msr     fpsr, %[fpsr]                     \n"
               : : [fpcr] "r" ((Mword)r->fpcr),
                   [fpsr] "r" ((Mword)r->fpsr),
                   [s] "r" (r->state),
                   "m" (r->state));
}

void
Fpu_arch::save_state(Fpu_state *fpu_regs)
{
  assert(fpu_regs);
  save_fpu_regs(fpu_regs);
}

void
Fpu_arch::restore_state(Fpu_state const *fpu_regs, bool)
{
  assert(fpu_regs);
  restore_fpu_regs(fpu_regs);
}

