#pragma once

#include <types.h>
#include <globalconfig.h>

template<typename DERIVED, typename BASE>
class Mem_arch : public BASE
{
public:
  static inline
  void memset_mwords (void *dst, unsigned long value, unsigned long n)
  {
    unsigned dummy1, dummy2;
    asm volatile ("cld					\n\t"
                  "repz stosl               \n\t"
                  : "=c"(dummy1), "=D"(dummy2)
                  : "a"(value), "c"(n), "D"(dst)
                  : "memory");
  }

  static inline
  void memcpy_bytes (void *dst, void const *src, unsigned long n)
  {
    unsigned dummy1, dummy2, dummy3;

    asm volatile ("cld					\n\t"
                  "repz movsl %%ds:(%%esi), %%es:(%%edi)	\n\t"
                  "mov %%edx, %%ecx			\n\t"
                  "repz movsb %%ds:(%%esi), %%es:(%%edi)	\n\t"
                  : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                  : "c" (n >> 2), "d" (n & 3), "S" (src), "D" (dst)
                  : "memory");
  }


  static inline
  void memcpy_mwords (void *dst, void const *src, unsigned long n)
  {
    unsigned dummy1, dummy2, dummy3;

    asm volatile ("cld					\n\t"
                  "rep movsl %%ds:(%%esi), %%es:(%%edi)	\n\t"
                  : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                  : "c" (n), "S" (src), "D" (dst)
                  : "memory");
  }

  static inline
  void memcpy_bytes_fs (void *dst, void const *src, unsigned long n)
  {
    unsigned dummy1, dummy2, dummy3;

    asm volatile ("cld					\n\t"
                  "rep movsl %%fs:(%%esi), %%es:(%%edi)	\n\t"
                  "mov %%edx, %%ecx			\n\t"
                  "repz movsb %%fs:(%%esi), %%es:(%%edi)	\n\t"
                  : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                  : "c" (n >> 2), "d" (n & 3), "S" (src), "D" (dst)
                  : "memory");
  }

  static inline
  void memcpy_mwords_fs (void *dst, void const *src, unsigned long n)
  {
    unsigned dummy1, dummy2, dummy3;

    asm volatile ("cld					\n\t"
                  "rep movsl %%fs:(%%esi), %%es:(%%edi)	\n\t"
                  : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3)
                  : "c" (n), "S" (src), "D" (dst)
                  : "memory");
  }

#ifdef CONFIG_MP
  inline static void mb()
  { __asm__ __volatile__ ("lock; addl $0,0(%%esp)" : : : "memory"); /* mfence */ }

  inline static void rmb() { mb(); /* lfence */ }
  inline static void wmb() { BASE::barrier(); /* sfence */}

  inline static void mp_mb() { mb(); }
  inline static void mp_acquire() { mb(); }
  inline static void mp_release() { mb(); }
  inline static void mp_rmb() { rmb(); }
  inline static void mp_wmb() { wmb(); }
#endif
};
