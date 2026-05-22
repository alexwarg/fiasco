#pragma once

#include <types.h>
#include <globalconfig.h>

template<typename DERIVED, typename BASE>
class Mem_arch : public BASE
{
public:
  static inline
  void sync()
  { asm volatile ("sync" : : : "memory"); }

  static inline
  void memset_mwords(void *dst, const unsigned long value, unsigned long nr_of_mwords)
  {
    unsigned long *d = (unsigned long *)dst;
    for (; nr_of_mwords--; d++)
      *d = value;
  }

  static inline
  void memcpy_mwords(void *dst, void const *src, unsigned long nr_of_mwords)
  {
    // FIXME: check if the compiler generates code that assumes Mword alignment
    __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
  }

  static inline
  void memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
  {
    __builtin_memcpy(dst, src, nr_of_bytes);
  }

#ifdef CONFIG_MP
#if !defined(CONFIG_WEAK_ORDERING)
  //-----------------------------------------------------------------------------
  inline static void mp_mb() { DERIVED::barrier(); }
  inline static void mp_rmb() { DERIVED::barrier(); }
  inline static void mp_wmb() { DERIVED::barrier(); }
  inline static void mp_acquire() { DERIVED::barrier(); }
  inline static void mp_release() { DERIVED::barrier(); }

#elif defined(CONFIG_WEAK_ORDERING) && defined(CONFIG_MIPS_LW_BARRIERS) \
        && !defined(CONFIG_CAVIUM_OCTEON)
  //-----------------------------------------------------------------------------
  inline static void mp_mb() { __asm__ __volatile__("sync 0x10" : : : "memory"); }
  inline static void mp_rmb() { __asm__ __volatile__("sync 0x13" : : : "memory"); }
  inline static void mp_wmb() { __asm__ __volatile__("sync 0x4" : : : "memory"); }
  inline static void mp_acquire() { __asm__ __volatile__("sync 0x11" : : : "memory"); }
  inline static void mp_release() { __asm__ __volatile__("sync 0x12" : : : "memory"); }

#elif defined(CONFIG_WEAK_ORDERING) && !defined(CONFIG_MIPS_LW_BARRIERS) \
        && !defined(CONFIG_CAVIUM_OCTEON)
  //-----------------------------------------------------------------------------
#if 0
  // Alex: I disable these implementations as the semantics are questionable and they are
  // not used in any MIPS code
  inline static void mb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void rmb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void wmb() { __asm__ __volatile__("sync" : : : "memory"); }
#endif

  inline static void mp_mb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_rmb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_wmb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_acquire() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_release() { __asm__ __volatile__("sync" : : : "memory"); }

#elif defined(CONFIG_WEAK_ORDERING) && defined(CONFIG_CAVIUM_OCTEON)
  //-----------------------------------------------------------------------------
  inline static void mp_mb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_rmb() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_wmb() { __asm__ __volatile__("syncw; syncw" : : : "memory"); }
  inline static void mp_acquire() { __asm__ __volatile__("sync" : : : "memory"); }
  inline static void mp_release() { __asm__ __volatile__("syncw; syncw" : : : "memory"); }
#endif
#endif // CONFIG_MP
};
