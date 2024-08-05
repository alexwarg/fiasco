#pragma once

#include <types.h>
#include <globalconfig.h>

template<typename DERIVED, typename BASE>
class Mem_arch : public BASE
{
public:
#if defined(CONFIG_BIT32) && ((defined(CONFIG_ARM_V7) && defined(CONFIG_MP)) || defined(CONFIG_ARM_V8))
  inline void  prefetch_w(void *addr)
  {
    asm volatile (".arch_extension mp\n"
         "pldw %0" : : "m"(*reinterpret_cast<char *>(addr)));
  }
#else
  inline void prefetch_w(void *) {}
#endif

#ifdef CONFIG_BIT32
  static inline
  void memset_mwords (void *dst, const unsigned long value, unsigned long nr_of_mwords)
  {
    if (!nr_of_mwords)
      return;

    typedef unsigned long __attribute__((may_alias)) U32;
    typedef unsigned long long __attribute__((may_alias)) U64;

    U32 *d32 = reinterpret_cast<U32 *>(dst);
    if ((unsigned long)d32 & 0x4U)
      {
        *d32++ = value;
        nr_of_mwords--;
      }

    U64 v64 = (U64{value} << 32) | value;
    for (; nr_of_mwords >= 4; d32 += 4, nr_of_mwords -= 4)
      {
        reinterpret_cast<U64 *>(d32)[0] = v64;
        reinterpret_cast<U64 *>(d32)[1] = v64;
      }

    if (nr_of_mwords & 0x2U)
      {
        *reinterpret_cast<U64 *>(d32) = v64;
        d32 += 2;
      }

    if (nr_of_mwords & 0x1U)
      *d32 = value;
  }

  static inline
  void memcpy_mwords (void *dst, void const *src, unsigned long nr_of_mwords)
  {
    unsigned long __attribute__((may_alias)) const *s = static_cast<unsigned long const *>(src);
    unsigned long __attribute__((may_alias)) *d = static_cast<unsigned long *>(dst);
    unsigned long tmp;

    if (__builtin_constant_p(nr_of_mwords))
      {
        // Exploit the fact that the length is a compile time constant to pick
        // the best inline assembly.
        switch (nr_of_mwords & 0x03U)
          {
          case 0:
            break;
          case 1:
            asm volatile("ldr %0, [%1], #4\n"
                         "str %0, [%2], #4\n"
                         : "=&r"(tmp), "+r"(s), "+r"(d)
                         : : "memory");
            break;
          case 2:
            asm volatile("ldm %0!, {r0, r1}\n"
                         "stm %1!, {r0, r1}\n"
                         : "+&r"(s), "+&r"(d)
                         : : "r0", "r1", "memory");
            break;
          case 3:
            asm volatile("ldm %0!, {r0, r1, r2}\n"
                         "stm %1!, {r0, r1, r2}\n"
                         : "+&r"(s), "+&r"(d)
                         : : "r0", "r1", "r2", "memory");
            break;
          }
        // nr_of_mwords &= ~0x03UL; not necessary because code below shifts out
        // the two LSBs anyway
      }
    else
      while (nr_of_mwords & 0x03U)
        {
          *d++ = *s++;
          nr_of_mwords--;
        }

    // Need to use inline assembly here because the compiler is not smart enough
    // to use ldm/stm. We could let the compiler choose the scratch registers but
    // it results in assembler warnings because the order of the registers is
    // sometimes not ascending. Using the caller-saved registers emitted the best
    // code overall.
    nr_of_mwords >>= 2;
    while (nr_of_mwords--)
      {
        asm volatile("ldm %0!, {r0, r1, r2, r3}\n"
                     "stm %1!, {r0, r1, r2, r3}\n"
                     : "+&r"(s), "+&r"(d)
                     : : "r0", "r1", "r2", "r3", "memory");
      }
  }

  static inline
  void memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
  {
    __builtin_memcpy(dst, src, nr_of_bytes);
  }
#endif
#ifdef CONFIG_BIT64
  static inline
  void memset_mwords (void *dst, const unsigned long value, unsigned long nr_of_mwords)
  {
    if (!nr_of_mwords)
      return;

    unsigned long __attribute__((may_alias)) *d = static_cast<unsigned long *>(dst);
    for (; nr_of_mwords >= 4; d += 4, nr_of_mwords -= 4U)
      {
        d[0] = value;
        d[1] = value;
        d[2] = value;
        d[3] = value;
      }

    if (nr_of_mwords & 0x2U)
      {
        d[0] = value;
        d[1] = value;
        d += 2;
      }

    if (nr_of_mwords & 0x1U)
      *d = value;
  }

  static inline
  void memcpy_mwords (void *dst, void const *src, unsigned long nr_of_mwords)
  {
    // The C-version is good enough.
    __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
  }

  static inline
  void memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
  {
    __builtin_memcpy(dst, src, nr_of_bytes);
  }
#endif

#if defined(CONFIG_ARM_V5)
  static inline void dmbst() { DERIVED::barrier(); }
  static inline void dmb() { DERIVED::barrier(); }
  static inline void isb() { DERIVED::barrier(); }
  static inline void dsb()
  { __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }

  static inline void dsbst() { dsb(); }
#elif defined(CONFIG_ARM_V6)
  static inline void dmbst()
  { __asm__ __volatile__ ("mcr p15, 0, r0, c7, c10, 5" : : : "memory"); }

  static inline void dmb()
  { __asm__ __volatile__ ("mcr p15, 0, r0, c7, c10, 5" : : : "memory"); }

  static inline void isb()
  { __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory"); }

  static inline void dsb()
  { __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory"); }

  static inline void dsbst() { dsb(); }
#elif defined(CONFIG_ARM_V7PLUS)
  static inline void dmbst()
  { __asm__ __volatile__ ("dmb ishst" : : : "memory"); }

  static inline void dmb()
  { __asm__ __volatile__ ("dmb ish" : : : "memory"); }

  static inline void isb()
  { __asm__ __volatile__ ("isb sy" : : : "memory"); }

  static inline void dsb()
  { __asm__ __volatile__ ("dsb ish" : : : "memory"); }

  static inline void dsbst()
  { __asm__ __volatile__ ("dsb ishst" : : : "memory"); }
#endif

#ifdef CONFIG_MP
  inline static void mb() { dmb(); }
  inline static void rmb() { dmb(); }
  inline static void wmb() { dmbst(); }

  inline static void mp_mb() { dmb(); }
  inline static void mp_acquire() { dmb(); }
  inline static void mp_release() { dmb(); }
  inline static void mp_rmb() { dmb(); }
  inline static void mp_wmb() { dmbst(); }
#endif

#if defined(CONFIG_BIT32) && ((defined(CONFIG_ARM_V7) && defined(CONFIG_ARM_LPAE)) \
    || defined(CONFIG_ARM_V8PLUS))

  template<typename T>
  static inline
  T read64_consistent(T const *t)
  {
    static_assert(sizeof(T) == sizeof(Unsigned64), "value has invalid size");

    T res;
    asm volatile ("ldrd %0, %H0, %1" : "=r" (res) : "m"(*t));
    return res;
  }

  template<typename T>
  static inline
  void write64_consistent(T *t, T val)
  {
    static_assert(sizeof(T) == sizeof(Unsigned64), "value has invalid size");
    asm volatile ("strd %1, %H1, %0" : "=m"(*t) : "r"(val));
  }
#endif
};
