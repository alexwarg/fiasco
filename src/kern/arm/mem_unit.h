#pragma once

#include "globalconfig.h"
#include "mem_layout.h"
#include "mmu.h"

// ---------------------------------------------------------------------------
// ASID width mixin

#ifdef CONFIG_ARM_ASID16
struct Mem_unit_asid
{
  enum { Asid_bits = 16 };
};
#else
struct Mem_unit_asid
{
  enum { Asid_bits = 8 };
};
#endif

// ---------------------------------------------------------------------------
// Arch-specific TLB implementation.
// Each header provides class Mem_unit_tlb with:
//   static void tlb_flush();
//   static void tlb_flush(unsigned long asid);
//   static void tlb_flush(void *va, unsigned long asid);
//   static void tlb_flush_kernel();
//   static void dtlb_flush(void *va);
//   static void make_coherent_to_pou(void const *start, size_t size);

#ifdef CONFIG_BIT32
# include "mem_unit-arm-32.h"
#else
# include "mem_unit-arm-64.h"
#endif

// ---------------------------------------------------------------------------
// Final Mem_unit: combines Mem_unit_tlb (arch TLB + coherency ops) with the
// ASID-width constant and the Asid_kernel / Asid_invalid sentinel values.
//
// TLB maintenance comments:
//
// User space TLB maintenance (tlb_flush variants):
//   Must only be used for user tasks. Guarantee maintenance has completed on
//   return. Note: completion of the I-cache side is assumed to be provided by
//   the subsequent return-to-user (context synchronization event).
//
// Kernel TLB maintenance (tlb_flush_kernel, dtlb_flush):
//   Must only be used for Kmem::kdir operations. Guarantee maintenance has
//   completed on return. Do not imply any branch predictor maintenance.

class Mem_unit
  : public Mem_unit_tlb,
    public Mem_unit_asid
{
public:
  enum : Mword
  {
    Asid_kernel  = 0UL,
    Asid_invalid = ~0UL
  };
};
