#pragma once

#include "globalconfig.h"
#include "std_macros.h"
#include "types.h"
#include <mmu-arch.h>

class Mmu_generic
{
public:
  /* start address is inclusive, end address is exclusive
   * (end = start + size) */
  /**
   * Clean the entire dcache.
   */
  static void clean_dcache() {}

  /**
   * Clean given D cache region.
   */
  // XXX: static void clean_dcache(void const *va) { (void)va; }

  /**
   * Clean given D cache region.
   */
  static void clean_dcache(void const *start, void const *end){ (void)start; (void)end; }

  /**
   * Clean and invalidate the entire cache.
   * D and I cache is cleaned and invalidated and the write buffer is
   * drained.
   */
  static void flush_cache() {}

  /**
   * Clean and invalidate the given cache region.
   * D and I cache are affected.
   */
  static void flush_cache(void const *start, void const *end) { (void)start; (void)end; }

  /**
   * Clean and invalidate the entire D cache.
   */
  static void flush_dcache() {}

  /**
   * Clean and invalidate the given D cache region.
   */
  static void flush_dcache(void const *start, void const *end) { (void)start; (void)end; }

  /**
   * Invalidate the given D cache region.
   */
  static void inv_dcache(void const *start, void const *end) { (void)start; (void)end; }

};

template<typename DERIVED>
class Mmu_non_vcache_mixin
{
public:
  static inline void flush_vcache(void const *, void const *) {}
  static void clean_vdcache(void const *, void const *) {}
  static void flush_vdcache(void const *, void const *) {}
  static void inv_vdcache(void const *, void const *) {}
  static void flush_vcache() {}
  static void clean_vdcache() {}
  static void flush_vdcache() {}
};

template<typename DERIVED>
class Mmu_vcache_mixin
{
public:
  static inline void flush_vcache(void const *start, void const *end)
  { DERIVED::flush_cache(start, end); }

  static void clean_vdcache(void const *start, void const *end)
  { DERIVED::clean_dcache(start, end); }

  static void flush_vdcache(void const *start, void const *end)
  { DERIVED::flush_dcache(start, end); }

  static void inv_vdcache(void const *start, void const *end)
  { DERIVED::inv_dcache(start, end); }

  static void flush_vcache()
  { DERIVED::flush_cache(); }

  static void clean_vdcache()
  { DERIVED::clean_dcache(); }

  static void flush_vdcache()
  { DERIVED::flush_dcache(); }
};

template< unsigned long Flush_area = 0, bool Ram = false >
class Mmu : public Mmu_arch<Mmu_generic, Flush_area, Ram>,
#ifdef CONFIG_VCACHE
          public Mmu_vcache_mixin<Mmu<Flush_area, Ram>>
#else
          public Mmu_non_vcache_mixin<Mmu<Flush_area, Ram>>
#endif
{
public:
  /**
   * Switch page table and do the necessary things.
   */
  static void switch_pdbr(Address base);
};
#if 0
// ---------------------------------------------------------------------------
// ARM arch-specific implementations (included after class definition is complete)

#ifdef CONFIG_ARM
# ifdef CONFIG_BIT32
#  include "mmu-arm-32.h"
# else
#  include "mmu-arm-64.h"
# endif
# include "mmu-arm.h"
#endif
#endif
