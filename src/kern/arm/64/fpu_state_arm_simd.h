
#pragma once

#include <types.h>
#include <mem.h>

class Fpu_state_simd
{
public:
  Unsigned32 fpcr, fpsr;
  Unsigned64 state[64]; // 32 128bit regs

  Fpu_state_simd()
  {
    static_assert(!(sizeof (*this) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(this, 0, sizeof (*this) / sizeof(Mword));
  }

  void save();
  void restore() const;
  void copy(Fpu_state_simd const *from)
  { *this = *from; }
};

inline void
Fpu_state_simd::save()
{
  asm volatile(".arch_extension fp                        \n"
               "stp     q0, q1,   [%[s], #16 *  0]        \n"
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
                 "=m" (state)
               : [s] "r" (state));
}

inline void
Fpu_state_simd::restore() const
{
  asm volatile(".arch_extension fp                        \n"
               "ldp     q0, q1,   [%[s], #16 *  0]        \n"
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
               : : [fpcr] "r" (Mword{fpcr}),
                   [fpsr] "r" (Mword{fpsr}),
                   [s] "r" (state),
                   "m" (state));
}

