#pragma once

#include "globalconfig.h"
#include "mem.h"
#include "mem_layout.h"
#include "mmu.h"
#include "types.h"

// Mixin providing all ARM 64-bit TLB and cache-coherency operations for
// Mem_unit.  Inherits btc_flush()/btc_inv() and cache-line query helpers from
// the Mmu template base.

class Mem_unit_tlb : public Mmu<Mem_layout::Cache_flush_area>
{
public:
  static void tlb_flush();
  static void tlb_flush(unsigned long asid);
  static void tlb_flush(void *va, unsigned long asid);
  static void tlb_flush_kernel();
  static void tlb_flush_kernel(Address va);
  static void make_coherent_to_pou(void const *start, size_t size);
};

// ---------------------------------------------------------------------------
#ifndef CONFIG_CPU_VIRT

inline void
Mem_unit_tlb::tlb_flush()
{
  Mem::dsbst();
  asm volatile("tlbi vmalle1is" : : : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long asid)
{
  if (asid == ~0UL)
    return;

  Mem::dsbst();
  asm volatile("tlbi vae1is, %0"
               : : "r" ((reinterpret_cast<unsigned long>(va) >> 12)
                        | (asid << 48)) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long asid)
{
  Mem::dsbst();
  asm volatile("tlbi aside1is, %0"
               : : "r" (asid << 48) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush_kernel()
{ tlb_flush(); }

inline void
Mem_unit_tlb::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("tlbi vaae1is, %0"
               : : "r" ((va >> 12) & 0x00000ffffffffffful)
               : "memory");
  Mem::dsb();
}

// ---------------------------------------------------------------------------
#else // CONFIG_CPU_VIRT

inline void
Mem_unit_tlb::tlb_flush()
{
  Mem::dsbst();
  asm volatile("tlbi alle1is" : : : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long asid)
{
  if (asid == ~0UL)
    return;

  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi ipas2e1is, %[ipa] \n"
      "dsb ish                \n"
      "tlbi vmalle1is         \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [ipa] "r" (reinterpret_cast<unsigned long>(va) >> 12),
      [asid] "r" (asid << 48)
      :
      "memory");
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long asid)
{
  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi vmalls12e1is      \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [asid] "r" (asid << 48)
      :
      "memory");
}

inline void
Mem_unit_tlb::tlb_flush_kernel()
{
  Mem::dsbst();
  asm volatile("tlbi alle2is" : : : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("tlbi vae2is, %0"
               : : "r" ((va >> 12) & 0x00000ffffffffffful)
               : "memory");
  Mem::dsb();
}

#endif // CONFIG_CPU_VIRT

// ---------------------------------------------------------------------------
// make_coherent_to_pou: cache-line-granular D+I maintenance to PoU (ARMv8)

inline void
Mem_unit_tlb::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long end = (unsigned long)start + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = (unsigned long)start & ~(ds - 1U); i < end; i += ds)
    __asm__ __volatile__ ("dc cvau, %0" : : "r"(i));

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = (unsigned long)start & ~(is - 1U); i < end; i += is)
    __asm__ __volatile__ ("ic ivau, %0" : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}
