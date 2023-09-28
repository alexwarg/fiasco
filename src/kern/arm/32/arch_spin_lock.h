#pragma once

#include "processor.h"

template<typename SL>
class Arch_spin_lock
{
public:
  enum { Arch_lock = 2 };

  void lock_arch() noexcept
  {
    static_assert(   sizeof(typename SL::Lock_type) == 1
                  || sizeof(typename SL::Lock_type) == 2
                  || sizeof(typename SL::Lock_type) == 4,
                  "unsupported spin-lock type for ARM");

    typename SL::Lock_type dummy, tmp;

#define LOCK_ARCH(z) \
    __asm__ __volatile__ ( \
        "1: ldr" #z "     %[d], [%[lock]]           \n" \
        "   tst     %[d], #2                  \n" /* Arch_lock == #2 */ \
        "   wfene                             \n" \
        "   bne     1b                        \n" \
        "   ldrex"#z"   %[d], [%[lock]]           \n" \
        "   tst     %[d], #2                  \n" \
        "   orr     %[tmp], %[d], #2          \n" \
        "   strex"#z"eq %[d], %[tmp], [%[lock]]   \n" \
        "   teqeq   %[d], #0                  \n" \
        "   bne     1b                        \n" \
        : [d] "=&r" (dummy), [tmp] "=&r"(tmp), "+m" (static_cast<SL *>(this)->_lock) \
        : [lock] "r" (&static_cast<SL *>(this)->_lock) \
        : "cc" \
        )

    switch(sizeof(typename SL::Lock_type))
      {
      case 1: LOCK_ARCH(b); break;
      case 2: LOCK_ARCH(h); break;
      case 4: LOCK_ARCH(); break;
      }

#undef LOCK_ARCH
  }

  void unlock_arch() noexcept
  {
    typename SL::Lock_type tmp;

#define UNLOCK_ARCH(z) \
    __asm__ __volatile__( \
        "ldr"#z " %[tmp], %[lock]             \n" \
        "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
        "str"#z " %[tmp], %[lock]             \n" \
        : [lock] "=m" (static_cast<SL *>(this)->_lock), [tmp] "=&r" (tmp)); \
    Mem::dsb(); \
    __asm__ __volatile__("sev")

    switch (sizeof(typename SL::Lock_type))
      {
      case 1: UNLOCK_ARCH(b); break;
      case 2: UNLOCK_ARCH(h); break;
      case 4: UNLOCK_ARCH(); break;
      }

#undef UNLOCK_ARCH
  }
};
