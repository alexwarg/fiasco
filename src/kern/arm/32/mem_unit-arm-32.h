#pragma once

#include "globalconfig.h"
#include "mem.h"
#include "mem_layout.h"
#include "mmu.h"
#include "types.h"

// Mixin providing all ARM 32-bit TLB and cache-coherency operations for
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
#if defined(CONFIG_ARM_V5)

// ARM v5: no ASID support in TLB ops

inline void
Mem_unit_tlb::tlb_flush()
{
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long)
{
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 1" // TLBIMVA
               : : "r" ((unsigned long)va & 0xfffff000) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long)
{ tlb_flush(); }

inline void
Mem_unit_tlb::tlb_flush_kernel()
{ tlb_flush(); }

inline void
Mem_unit_tlb::tlb_flush_kernel(Address va)
{
  // No ASIDs on ARMv5, so just use the regular tlb_flush() implementation
  // passing a dummy ASID value that is ignored anyway.
  tlb_flush(va, 0);
}

// ---------------------------------------------------------------------------
#elif defined(CONFIG_CPU_VIRT) // ARM v7plus with hypervisor support

inline void
Mem_unit_tlb::tlb_flush()
{
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 4, r0, c8, c3, 4" : : : "memory"); // TLBIALLNSNHIS
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long asid)
{
  if (asid == ~0UL)
    return;
  Mmu::btc_flush();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n" // save VTTBR
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n" // write VMID to VTTBR
      "isb \n"
      "dsb ishst \n"
      "mcr  p15, 0, %[mva], c8, c3, 3 \n" // TLBIMVAAIS
      "dsb ish \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n" // restore VTTBR
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [mva] "r" ((unsigned long)va & 0xfffff000), [asid] "r" (asid << 16)
      : "memory");
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long asid)
{
  Mmu::btc_flush();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n" // save VTTBR
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n" // write VMID to VTTBR
      "isb \n"
      "dsb ishst \n"
      "mcr  p15, 0, %[tmp1], c8, c3, 0 \n" // TLBIALLIS
      "dsb ish \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n" // restore VTTBR
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [asid] "r" (asid << 16)
      : "memory");
}

inline void
Mem_unit_tlb::tlb_flush_kernel()
{
  Mem::dsbst();
  asm volatile("mcr p15, 4, r0, c8, c3, 0" : : : "memory"); // TLBIALLHIS
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("mcr p15, 4, %0, c8, c3, 1" // TLBIMVAHIS
               : : "r" (va & 0xfffff000) : "memory");
  Mem::dsb();
}

// ---------------------------------------------------------------------------
#elif defined (CONFIG_ARM_V6) || (defined (CONFIG_ARM_V7) && ! defined (CONFIG_MP))
// ARM v6 or v7 w/o MP and w/o CPU virtualisation

inline void
Mem_unit_tlb::tlb_flush()
{
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long asid)
{
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 2" // TLBIASID
               : : "r" (asid) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long asid)
{
  if (asid == ~0UL /*Asid_invalid*/)
    return;
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 1" // TLBIMVA
               : : "r" ((reinterpret_cast<Address>(va) & 0xfffff000) | asid) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush_kernel()
{ tlb_flush(); }

inline void
Mem_unit_tlb::tlb_flush_kernel(Address)
{
  // On ARMv6 and ARMv7 without multiprocessing extension, it is not possible to
  // flush all non-global TLB entries for an address without considering the
  // associated ASID, thus perform a full TLB flush.
  tlb_flush_kernel();
}

#else
// ARM v6plus MP without CPU virtualisation

inline void
Mem_unit_tlb::tlb_flush()
{
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 0" // TLBIALLIS
               : : "r" (0) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(void *va, unsigned long asid)
{
  if (asid == ~0UL)
    return;
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 1" // TLBIMVAIS
               : : "r" ((reinterpret_cast<Address>(va) & 0xfffff000) | asid) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush(unsigned long asid)
{
  Mmu::btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 2" // TLBIASIDIS
               : : "r" (asid) : "memory");
  Mem::dsb();
}

inline void
Mem_unit_tlb::tlb_flush_kernel()
{ tlb_flush(); }

inline void
Mem_unit_tlb::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 3" // TLBIMVAAIS
               : : "r" (va & 0xfffff000) : "memory");
  Mem::dsb();
}

#endif // ARM v5 / cpu_virt / v6plus

// ---------------------------------------------------------------------------
// make_coherent_to_pou: cache-line-granular D+I maintenance to PoU

#ifdef CONFIG_ARM_V7PLUS

inline ALWAYS_INLINE void
Mem_unit_tlb::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long end = (unsigned long)start + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = (unsigned long)start & ~(ds - 1U); i < end; i += ds)
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c11, 1" : : "r"(i));  // DCCMVAU

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = (unsigned long)start & ~(is - 1U); i < end; i += is)
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
        "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
        : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}

#else // !CONFIG_ARM_V7PLUS

inline ALWAYS_INLINE void
Mem_unit_tlb::make_coherent_to_pou(void const *start, size_t size)
{
  // This does more than necessary: It writes back + invalidates the data
  // cache instead of cleaning. But this function shall not be used in
  // performance critical code anyway.
  flush_cache(start, static_cast<Unsigned8 const *>(start) + size);
}

#endif // CONFIG_ARM_V7PLUS
