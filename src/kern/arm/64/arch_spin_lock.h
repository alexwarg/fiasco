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
                  || sizeof(typename SL::Lock_type) == 4
                  || sizeof(typename SL::Lock_type) == 8,
                  "unsupported spin-lock type for ARM");

    using Lock_t = typename SL::Lock_type;
    Lock_t dummy, tmp;

#define LOCK_ARCH(z,u) \
    __asm__ __volatile__ ( \
        "   sevl                                      \n" \
        "   prfm pstl1keep, [%[lock]]                 \n" \
        "1: wfe                                       \n" \
        "   ldaxr" #z "  %" #u "[d], [%[lock]]        \n" \
        "   tst     %x[d], #2                         \n" /* Arch_lock == #2 */ \
        "   bne 1b                                    \n" \
        "   orr   %x[tmp], %x[d], #2                  \n" \
        "   stxr" #z " %w[d], %" #u "[tmp], [%[lock]] \n" \
        "   cbnz  %w[d], 1b                           \n" \
        : [d] "=&r" (dummy), [tmp] "=&r"(tmp), "+m" (static_cast<SL *>(this)->_lock) \
        : [lock] "r" (&static_cast<SL *>(this)->_lock) \
        : "cc" \
        )

    switch (sizeof(Lock_t))
      {
      case 1: LOCK_ARCH(b,w); break;
      case 2: LOCK_ARCH(h,w); break;
      case 4: LOCK_ARCH(,w); break;
      case 8: LOCK_ARCH(,x); break;
      }

#undef LOCK_ARCH
  }

  void unlock_arch() noexcept
  {
    using Lock_t = typename SL::Lock_type;
    Lock_t tmp;

#define UNLOCK_ARCH(z,u) \
    __asm__ __volatile__( \
        "ldr"#z " %" #u "[tmp], %[lock]              \n" \
        "bic %x[tmp], %x[tmp], #2                    \n" /* Arch_lock == #2 */ \
        "stlr"#z " %" #u "[tmp], %[lock]             \n" \
        : [lock] "+Q" (static_cast<SL *>(this)->_lock), [tmp] "=&r" (tmp))

    switch (sizeof(Lock_t))
      {
      case 1: UNLOCK_ARCH(b,w); break;
      case 2: UNLOCK_ARCH(h,w); break;
      case 4: UNLOCK_ARCH(,w); break;
      case 8: UNLOCK_ARCH(,x); break;
      }

#undef UNLOCK_ARCH
  }
};
