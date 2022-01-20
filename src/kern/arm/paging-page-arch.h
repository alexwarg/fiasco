#pragma once

#include <types.h>
#include <mem_unit.h>
#include <globalconfig.h>

namespace Page
{
#ifdef CONFIG_ARM_V5
  enum User_attr
  {
    Cache_mask    = 0x0c,
    NONCACHEABLE  = 0x00, ///< Caching is off
    CACHEABLE     = 0x0c, ///< Cache is enabled

    // The next are ARM specific
    WRITETHROUGH = 0x08, ///< Write through cached
    BUFFERED     = 0x04, ///< Write buffer enabled
  };

  enum Default_entries : Mword
  {
    Section_cachable = 0x40e,
    Section_no_cache = 0x402,
    Section_local    = 0,
    Section_global   = 0,
  };
#endif // CONFIG_ARM_V5

#if (defined(CONFIG_ARM_V6PLUS) && defined(CONFIG_MP)) || defined (CONFIG_ARM_V8)
  static constexpr Mword Section_shared = 1UL << 16;
  static constexpr Mword _Mp_set_shared = 0x400;
#endif // (CONFIG_ARM_V6PLUS && CONFIG_MP) || CONFIG_ARM_V8
#if (defined(CONFIG_ARM_V6) || defined(CONFIG_ARM_V7)) && !defined(CONFIG_MP)
  static constexpr Mword Section_shared = 0;
  static constexpr Mword _Mp_set_shared = 0;
#endif // (CONFIG_ARM_V6 || CONFIG_ARM_V7) && !CONFIG_MP
#ifdef CONFIG_ARM_V5
  static constexpr Mword Ttbr_bits = 0;
#endif
#if defined (CONFIG_ARM_V6) && !defined(CONFIG_ARM_MPCORE)
  static constexpr Mword Ttbr_bits = 0;
#endif
#ifdef CONFIG_ARM_MPCORE
  static constexpr Mword Ttbr_bits = 0xa;
#endif

#ifdef CONFIG_ARM_LPAE
  static constexpr Mword Ttbr_bits = 0;
  /// Attributes for page-table walks
  static constexpr Mword Tcr_attribs =  (3UL << 4)  // SH0
                                      | (1UL << 2)  // ORGN0
                                      | (1UL << 0); // IRGN0

  /**
   * Memory Attribute Indirection (MAIR0)
   * Attr0: Device-nGnRnE memory
   * Attr1: Normal memory, Inner/Outer Non-cacheable
   * Attr2: Normal memory, RW, Inner/Outer Write-Back Cacheable (Non-transient)
   */
  static constexpr Mword Mair0_prrr_bits = 0x00ff4400;
  static constexpr Mword Mair1_nmrr_bits = 0;

#ifdef CONFIG_BIT32
  static constexpr Mword Ttbcr_bits =   (1 << 31) // EAE
                                      | (Tcr_attribs << 8);
#endif

#ifdef CONFIG_BIT64
#ifdef CONFIG_CPU_VIRT
  static constexpr Mword Ttbcr_bits =
      (1UL << 31) | (1UL << 23) // RES1
    | (Tcr_attribs <<  8) // (IRGN0)
    | (16UL <<  0)  // (T0SZ) Address space size 48bits (64 - 16)
    | (0UL  << 14)  // (TG0)  Page granularity 4kb
    | (5UL  << 16); // (PS)   Physical address size 48bits
                    //
#ifdef CONFIG_ARM_PT48
  static constexpr Mword Max_pa_range = 5; // 48 bits PA/IPA size (encoded as VTCR_EL2.PS)
  static constexpr Mword Vtcr_sl0 = 2;     // 4 level page table
#else
  static constexpr Mword Max_pa_range = 2; // 40 bits PA/IPA size (encoded as VTCR_EL2.PS)
  static constexpr Mword Vtcr_sl0 = 1;     // 3 level page table
#endif
  static unsigned inline ipa_bits(unsigned pa_range)
  {
    static char const pa_range_bits[] = { 32, 36, 40, 42, 44, 48, 52 };
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    return pa_range_bits[pa_range];
  }

  static unsigned inline vtcr_bits(unsigned pa_range)
  {
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    unsigned pa_bits = ipa_bits(pa_range);

    return (Vtcr_sl0            <<  6)  // SL0
            | (pa_range         << 16)  // PS
            | ((64U - pa_bits)  <<  0)  // T0SZ
            | ((Mem_unit::Asid_bits == 16) << 19); // VS
  }

#else // CONFIG_CPU_VIRT
  static constexpr Mword Ttbcr_bits =
      (Tcr_attribs <<  8) // (IRGN0)
    | (Tcr_attribs << 24) // (IRGN1)
    | (16UL <<  0) // (T0SZ) Address space size 48bits (64 - 16)
    | (16UL << 16) // (T1SZ) Address space size 48bits (64 - 16)
    | (0UL  << 14) // (TG0)  Page granularity 4kb
    | (2UL  << 30) // (TG1)  Page granularity 4kb
    | (5UL  << 32) // (IPS)  Physical address size 48bits
                   // (AS)   ASID Size
    | ((Mem_unit::Asid_bits == 16 ? 1UL : 0UL) << 36);
#endif // CONFIG_CPU_VIRT
#endif // CONFIG_BIT64

#ifdef CONFIG_CPU_VIRT
  enum User_attr
  {
    Mp_set_shared = _Mp_set_shared,
    Cache_mask    = 0x03c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x03c, ///< Cache is enabled
    BUFFERED      = 0x014, ///< Write buffer enabled -- Normal, non-cached
  };

  struct Kernel_attr
  { enum {
    Cache_mask    = 0x01c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x008, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  }; };
#else // CONFIG_CPU_VIRT
  enum User_attr
  {
    Mp_set_shared = _Mp_set_shared,
    Cache_mask    = 0x01c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x008, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  };

  using Kernel_attr = User_attr;
#endif // CONFIG_CPU_VIRT
#else // CONFIG_ARM_LPAE

#if defined (CONFIG_ARM_V6) && ! defined (CONFIG_ARM_MPCORE)
  enum User_attr
  {
    Cache_mask    = 0x1cc,
    Mp_set_shared = _Mp_set_shared,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x144, ///< Cache is enabled
    BUFFERED      = 0x040, ///< Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 0x5004,
  };
#endif // CONFIG_ARM_V6 && !CONFIG_ARM_MPCORE

#if defined (CONFIG_ARM_V7PLUS) || defined (CONFIG_ARM_MPCORE)
  enum User_attr
  {
    Cache_mask    = 0x1cc,
    Mp_set_shared = _Mp_set_shared,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x008, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 8,
  };
#endif // CONFIG_ARM_V7PLUS || CONFIG_ARM_MPCORE

#ifdef CONFIG_ARM_V6PLUS
  enum : Mword
  {
    Section_cache_mask = 0x700c,
    Section_local      = (1 << 17),
    Section_global     = 0,
    Section_cachable   = 0x0402 | Section_shared | Section_cachable_bits,
    Section_no_cache   = 0x0402 | Section_shared | 0x10 /*XN*/,
  };
#endif // CONFIG_ARM_V6PLUS

#ifdef CONFIG_ARM_V7
#ifdef CONFIG_MP
  // S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS
  static constexpr Mword Ttbr_bits = 0x6a;
#else // CONFIG_MP
  // armv7 w/o multiprocessing ext.
  // RGN = Outer WB-WA | IRGN = Inner WB-WA
  static constexpr Mword Ttbr_bits = 0x09;
#endif // CONFIG_MP
#endif // CONFIG_ARM_V7

#ifdef CONFIG_ARM_V8
  // S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS
  static constexpr Mword Ttbr_bits = 0x6a;
#endif // CONFIG_ARM_V8

  static constexpr Mword Ttbcr_bits      = 0;
  /**
   * Primary Region Remap (PRRR)
   * TR0, NOS0: Device-nGnRnE memory, Inner Shareable
   * TR1, NOS1: Normal memory, Inner Shareable
   * TR2, NOS2: Normal memory, Inner Shareable
   */
  static constexpr Mword Mair0_prrr_bits = 0xff0a0028;
  /**
   * Normal Memory Remap (NMRR)
   * IR1/OR1: Inner/Outer Non-cacheable
   * IR2/OR2: Inner/Outer Write-Back Write-Allocate Cacheable
   */
  static constexpr Mword Mair1_nmrr_bits = 0x00100010;
#endif // CONFIG_ARM_LPAE
}
