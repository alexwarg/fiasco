#pragma once

#include "processor.h"

template<typename SL>
class Arch_spin_lock
{
public:
  enum { Arch_lock = 2 };

  void lock_arch() noexcept
  {
    typename SL::Lock_type dummy, tmp;

#define L(z) \
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
    extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
    switch(sizeof(typename SL::Lock_type))
      {
      case 1: L(b); break;
      case 2: L(h); break;
      case 4: L(); break;
      default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 10; break;
      }

#undef L
  }

  void unlock_arch() noexcept
  {
    typename SL::Lock_type tmp;
#define UNL(z) \
    __asm__ __volatile__( \
        "ldr"#z " %[tmp], %[lock]             \n" \
        "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
        "str"#z " %[tmp], %[lock]             \n" \
        : [lock] "=m" (static_cast<SL *>(this)->_lock), [tmp] "=&r" (tmp)); \
    Mem::dsb(); \
    __asm__ __volatile__("sev")
    extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
    switch (sizeof(typename SL::Lock_type))
      {
      case 1: UNL(b); break;
      case 2: UNL(h); break;
      case 4: UNL(); break;
      default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 11; break;
      }
#undef UNL
  }
};

