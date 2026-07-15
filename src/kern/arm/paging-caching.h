#pragma once

#include <globalconfig.h>
#include <mem_unit.h>

struct Pte_cache_base
{
  static void write_back(void *start, void *end)
  {
    Mem_unit::clean_dcache(start, end);
  }
};

/**
 * Mixin for PTE pointers for CPUs with virtual caches and without ASIDs.
 * (before and including ARMv5)
 */
struct Pte_v_cache_no_asid : Pte_cache_base
{
  static bool need_cache_write_back(bool current_pt)
  { return current_pt; }

  template<typename T>
  static void write_back_if(T const &pte, bool current_pt, Mword /*asid*/ = 0)
  {
    if (current_pt)
      Mem_unit::clean_dcache(pte.pte);
  }
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and non-coherent MMU.
 * (ARMv6 and ARMv7 without multiprocessing extension).
 */
struct Pte_cache_asid : Pte_cache_base
{
  static bool need_cache_write_back(bool)
  { return true; }

  template<typename T>
  static void write_back_if(T const &pte, bool, Mword asid = Mem_unit::Asid_invalid)
  {
    Mem_unit::clean_dcache(pte.pte);
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }
};

struct Pte_no_cache_base
{
  static void write_back(void *, void *)
  {}
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and coherent MMU.
 * (ARMv7 with multiprocessing extension or LPAE and ARMv8).
 */
struct Pte_no_cache_asid : Pte_no_cache_base
{
  static bool need_cache_write_back(bool)
  { return false; }

  template<typename T>
  static void write_back_if(T const &, bool, Mword asid = Mem_unit::Asid_invalid)
  {
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }
};


