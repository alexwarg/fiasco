#pragma once

#include <globalconfig.h>
#include <processor.h>
#include <mem.h>

template<typename GEN, unsigned long Flush_area = 0, bool Ram = false>
class Mmu_arm_bits : public GEN
{
public:
#if defined(CONFIG_ARM_V5)
  static inline void btc_flush() {}
  static inline void btc_inv() {}

#elif defined(CONFIG_ARM_V6PLUS)
  static inline void btc_flush()
  { asm volatile ("mcr p15, 0, %0, c7, c5, 6" : : "r" (0) : "memory"); }

  static inline void btc_inv()
  { asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory"); }

  // ---------------------------------------------------------------------------
  // ARM v6plus: range flush_cache, clean_dcache, flush_dcache, inv_dcache
  static inline void flush_cache(void const *start, void const *end)
  {
    unsigned long s = (unsigned long)start;
    unsigned long e = (unsigned long)end;
    unsigned long is = icache_line_size(), ds = dcache_line_size();

    for (unsigned long i = s & ~(ds - 1U); i < e; i += ds)
      __asm__ __volatile__ ("mcr p15, 0, %0, c7, c14, 1" : : "r"(i));  // DCCIMVAC

    Mem::dsb(); // make sure data cache changes are visible to instruction cache

    for (unsigned long i = s & ~(is - 1U); i < e; i += is)
      __asm__ __volatile__ (
          "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
          "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
          : : "r"(i));

    Mem::dsb(); // ensure completion of instruction cache invalidation
  }

  static inline void clean_dcache(void const *va)
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c10, 1       \n" // DCCMVAC
        :
        : "r" ((unsigned long)va & ~(dcache_line_size() - 1))
        : "memory");
  }

  static inline void clean_dcache(void const *start, void const *end)
  {
    Mem::dsb();
    __asm__ __volatile__ (
        // arm1176 only: "    mcrr p15, 0, %2, %1, c12         \n"
        "1:  mcr p15, 0, %[i], c7, c10, 1   \n" // DCCMVAC
        "    add %[i], %[i], %[clsz]        \n"
        "    cmp %[i], %[end]               \n"
        "    blo 1b                         \n"
        : [i]     "=&r" (start)
        :         "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
          [end]   "r"   (end),
          [clsz]  "ir"  (dcache_line_size())
        : "memory");
    btc_inv();
    Mem::dsb();
  }

  FIASCO_NOINLINE static void flush_dcache(void const *start, void const *end)
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "1:  mcr p15, 0, %[i], c7, c14, 1 \n" // Clean and Invalidate Data Cache Line (using MVA) Register
        "    add %[i], %[i], %[clsz]      \n"
        "    cmp %[i], %[end]             \n"
        "    blo 1b                       \n"
        : [i]    "=&r" (start)
        :        "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
          [end]  "r"   (end),
          [clsz] "ir"  (dcache_line_size())
        : "memory");
    btc_inv();
    Mem::dsb();
  }

  FIASCO_NOINLINE static void inv_dcache(void const *start, void const *end)
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "1:  mcr p15, 0, %[i], c7, c6, 1  \n" // Invalidate Data Cache Line (using MVA) Register
        "    add %[i], %[i], %[clsz]      \n"
        "    cmp %[i], %[end]             \n"
        "    blo 1b                       \n"
        : [i]    "=&r" (start)
        :        "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
          [end]  "r"   (end),
          [clsz] "ir"  (dcache_line_size())
        : "memory");
    btc_inv();
    Mem::dsb();
  }
#endif // CONFIG_ARM_V6PLUS

  // ---------------------------------------------------------------------------
  // Cache line size queries
#if defined(CONFIG_ARM_V7) || defined(CONFIG_ARM_V8)
  static inline Mword dcache_line_size()
  {
    Mword v;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v));
    return 4 << ((v >> 16) & 0xf);
  }

  static inline Mword icache_line_size()
  {
    Mword v;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v));
    return 4 << (v & 0xf);
  }

protected:
  static inline Mword get_clidr()
  {
    Mword clidr;
    asm volatile("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr));
    return clidr;
  }

  static inline Mword get_ccsidr(Mword csselr)
  {
    Mword ccsidr;
    Proc::Status s = Proc::cli_save();
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr));
    Mem::isb();
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
    Proc::sti_restore(s);
    return ccsidr;
  }

  static inline void dc_cisw(Mword v)
  {
    asm volatile("mcr p15, 0, %0, c7, c14, 2" : : "r" (v) : "memory");
  }

  static inline void dc_csw(Mword v)
  {
    asm volatile("mcr p15, 0, %0, c7, c10, 2" : : "r" (v) : "memory");
  }

  static inline void ic_iallu()
  {
    asm volatile("mcr p15, 0, r0, c7, c5, 0" : : : "memory");
  }

public:
#elif defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_1136) \
   || defined(CONFIG_ARM_1176) || defined(CONFIG_ARM_PXA) \
   || defined(CONFIG_ARM_SA)   || defined(CONFIG_ARM_926) \
   || defined(CONFIG_ARM_920T)

  static constexpr Mword dcache_line_size()
  { return 32; }

  static constexpr Mword icache_line_size()
  { return 32; }

#endif

  // ---------------------------------------------------------------------------
  // ARM mpcore / 1136 / 1176: whole-cache flush_cache, clean_dcache, flush_dcache
#if defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_1136) || defined(CONFIG_ARM_1176)

  static void flush_cache()
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
        "    mcr p15, 0, r0, c7, c5, 0        \n" // Invalidate Entire Instruction Cache Register
        : : : "memory");
    btc_inv();
  }

  static void clean_dcache()
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "    mcr p15, 0, r0, c7, c10, 0       \n" // Clean Entire Data Cache Register
        : : : "memory");
    btc_inv();
  }

  static void flush_dcache()
  {
    Mem::dsb();
    __asm__ __volatile__ (
        "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
        : : : "memory");
    btc_inv();
  }
#endif // CONFIG_ARM_MPCORE || CONFIG_ARM_1136 || CONFIG_ARM_1176

#if defined(CONFIG_ARM_920T)
  // ---------------------------------------------------------------------------
  // ARM 920T: whole-cache and range ops

  static inline void flush_cache(void const * /*start*/, void const * /*end*/)
  { flush_cache(); }

  FIASCO_NOINLINE static void clean_dcache(void const *start, void const *end)
  { (void)start; (void)end; clean_dcache(); }

  static void clean_dcache(void const *va)
  { (void)va; clean_dcache(); }

  FIASCO_NOINLINE static void flush_dcache(void const *start, void const *end)
  { (void)start; (void)end; flush_dcache(); }

  FIASCO_NOINLINE static void inv_dcache(void const *start, void const *end)
  {
    (void)start; (void)end;
#if 1
    for (unsigned long index = 0; index < (1 << (32 - 26)); ++index)
      for (unsigned long seg = 0; seg < 256; seg += 32)
        asm volatile("mcr p15,0,%0,c7,c14,2" : : "r" ((index << 26) | seg));
#else
    asm volatile("mcr p15,0,%0,c7,c6,0" : : "r" (0) : "memory");
#endif
  }

  FIASCO_NOINLINE static void flush_cache()
  {
    Mem::dsb();
    for (unsigned long index = 0; index < (1 << (32 - 26)); ++index)
      for (unsigned long seg = 0; seg < 256; seg += 32)
        asm volatile("mcr p15,0,%0,c7,c14,2" : : "r" ((index << 26) | seg));
    asm volatile("mcr p15,0,%0,c7,c5,0" : : "r" (0) : "memory");
  }

  FIASCO_NOINLINE static void clean_dcache()
  { flush_cache(); }

  FIASCO_NOINLINE static void flush_dcache()
  { flush_cache(); }

#endif // CONFIG_ARM_920T

#if defined(CONFIG_ARM_PXA) || defined(CONFIG_ARM_SA) || defined(CONFIG_ARM_926)
  // ---------------------------------------------------------------------------
  // ARM PXA / SA / 926: range ops

  static inline void flush_cache(void const * /*start*/, void const * /*end*/)
  { flush_cache(); }

  FIASCO_NOINLINE static void clean_dcache(void const *start, void const *end)
  {
    if (((Address)end) - ((Address)start) >= 8192)
      clean_dcache();
    else
      {
        asm volatile (
            "    bic  %0, %0, %2 - 1         \n"
            "1:  mcr  p15, 0, %0, c7, c10, 1 \n"
            "    add  %0, %0, %2             \n"
            "    cmp  %0, %1                 \n"
            "    blo  1b                     \n"
            "    mcr  p15, 0, %0, c7, c10, 4 \n" // drain WB
            : : "r" (start), "r" (end), "i" (dcache_line_size())
            );
      }
  }

  static void clean_dcache(void const *va)
  {
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 1       \n"
                          : : "r"(va) : "memory");
  }

  FIASCO_NOINLINE static void flush_dcache(void const *start, void const *end)
  {
    if (((Address)end) - ((Address)start) >= 8192)
      flush_dcache();
    else
      {
        asm volatile (
            "    bic  %0, %0, %2 - 1         \n"
            "1:  mcr  p15, 0, %0, c7, c14, 1 \n"
            "    add  %0, %0, %2             \n"
            "    cmp  %0, %1                 \n"
            "    blo  1b                     \n"
            "    mcr  p15, 0, %0, c7, c10, 4 \n" // drain WB
            : : "r" (start), "r" (end), "i" (dcache_line_size())
            );
      }
  }

  FIASCO_NOINLINE static void inv_dcache(void const *start, void const *end)
  {
    asm volatile (
        "    bic  %0, %0, %2 - 1         \n"
        "1:  mcr  p15, 0, %0, c7, c6, 1  \n"
        "    add  %0, %0, %2             \n"
        "    cmp  %0, %1                 \n"
        "    blo  1b                     \n"
        : : "r" (start), "r" (end), "i" (dcache_line_size())
        );
  }
#endif // CONFIG_ARM_PXA || CONFIG_ARM_SA || CONFIG_ARM_926

#if defined(CONFIG_ARM_SA)
  // ---------------------------------------------------------------------------
  // ARM SA: whole-cache flush_cache, clean_dcache, flush_dcache

  FIASCO_NOINLINE static void flush_cache()
  {
    Mword dummy;
    asm volatile (
        "     add %0, %1, #8192           \n" // 8k flush area
        " 1:  ldr r0, [%1], %2            \n" // 32 bytes cache line size
        "     teq %1, %0                  \n"
        "     bne 1b                      \n"
        "     mov r0, #0                  \n"
        "     mcr  p15, 0, r0, c7, c7, 0  \n"
        "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
        : "=r" (dummy)
        : "r" (Flush_area), "i" (dcache_line_size())
        : "r0"
        );
  }

  FIASCO_NOINLINE static void clean_dcache()
  {
    Mword dummy;
    asm volatile (
        "     add %0, %1, #8192 \n" // 8k flush area
        " 1:  ldr r0, [%1], %2  \n"
        "     teq %1, %0        \n"
        "     bne 1b            \n"
        "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
        : "=r" (dummy)
        : "r" (Flush_area), "i" (dcache_line_size())
        : "r0"
        );
  }

  FIASCO_NOINLINE static void flush_dcache()
  {
    Mword dummy;
    asm volatile (
        "     add %0, %1, #8192           \n" // 8k flush area
        " 1:  ldr r0, [%1], %2            \n"
        "     teq %1, %0                  \n"
        "     bne 1b                      \n"
        "     mov  r0, #0                 \n"
        "     mcr  p15, 0, r0, c7, c6, 0  \n" // inv D cache
        "     mcr  p15, 0, r0, c7, c10, 4 \n" // drain WB
        : "=r" (dummy)
        : "r" (Flush_area), "i" (dcache_line_size())
        : "r0"
        );
  }
#endif // CONFIG_ARM_SA

#if defined(CONFIG_ARM_PXA)
  // ---------------------------------------------------------------------------
  // ARM PXA: whole-cache flush_cache, clean_dcache, flush_dcache

  FIASCO_NOINLINE static void flush_cache()
  {
    Mword dummy1, dummy2;
    asm volatile
      (
       " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
       "    add %0, %0, #32                                \n\t"
       "    subs %1, %1, #1                                \n\t"
       "    bne 1b                                         \n\t"
       "    mcr  p15, 0, %0, c7, c7, 0                     \n\t"
       "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
       :
       "=r" (dummy1),
       "=r" (dummy2)
       :
       "0" (Flush_area),
       "1" (Ram ? 2048 : 1024)
      );
  }

  FIASCO_NOINLINE static void clean_dcache()
  {
    Mword dummy1, dummy2;
    asm volatile
      (
       " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
       "    add %0, %0, #32                                \n\t"
       "    subs %1, %1, #1                                \n\t"
       "    bne 1b                                         \n\t"
       "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
       :
       "=r" (dummy1),
       "=r" (dummy2)
       :
       "0" (Flush_area),
       "1" (Ram ? 2048 : 1024)
      );
  }

  FIASCO_NOINLINE static void flush_dcache()
  {
    Mword dummy1, dummy2;
    asm volatile
      (
       " 1: mcr p15, 0, %0, c7, c2, 5                      \n\t"
       "    add %0, %0, #32                                \n\t"
       "    subs %1, %1, #1                                \n\t"
       "    bne 1b                                         \n\t"
       "    mcr  p15, 0, %0, c7, c6, 0                     \n\t" // inv D cache
       "    mcr p15, 0, r0, c7, c10, 4                     \n\t"
       :
       "=r" (dummy1),
       "=r" (dummy2)
       :
       "0" (Flush_area),
       "1" (Ram ? 2048 : 1024)
      );
  }
#endif // CONFIG_ARM_PXA

#if defined(CONFIG_ARM_926)
  // ---------------------------------------------------------------------------
  // ARM 926: whole-cache flush_cache, clean_dcache, flush_dcache
  FIASCO_NOINLINE static void flush_cache()
  {
    asm volatile
      (
       "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
       "    bne 1b                                         \n\t"
       "    mcr p15, 0, %0, c7, c7, 0                      \n\t"
       "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
       : :
       "r" (0)
      );
  }

  FIASCO_NOINLINE static void clean_dcache()
  {
    asm volatile
      (
       "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
       "    bne 1b                                         \n\t"
       "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
       : :
       "r" (0)
      );
  }

  FIASCO_NOINLINE static void flush_dcache()
  {
    asm volatile
      (
       "1:  mrc p15, 0, r15, c7, c14, 3                    \n\t"
       "    bne 1b                                         \n\t"
       "    mcr  p15, 0, %0, c7, c6, 0                     \n\t" // inv D cache
       "    mcr p15, 0, %0, c7, c10, 4                     \n\t"
       : :
       "r" (0)
      );
  }
#endif // CONFIG_ARM_926
};
