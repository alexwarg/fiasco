#pragma once

#include "types.h"
#include "processor.h"

class Mem_unit
{
public:
  enum : unsigned short
  {
    Asid_invalid = 0xffff,
    Asid_mask    = 0x3ff,
  };

  enum : Mword
  {
    Entry_hi_EHINV = 1U << 10
  };

  static void index_reg(Signed32 v)
  { Mips::mtc0_32(v, Mips::Cp0_index); }

  static Signed32 index_reg()
  { return Mips::mfc0_32(Mips::Cp0_index); }

  static void entry_lo0(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_lo1); }

  static void entry_lo1(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_lo2); }

  static Mword entry_lo0()
  { return Mips::mfc0(Mips::Cp0_entry_lo1); }

  static Mword entry_lo1()
  { return Mips::mfc0(Mips::Cp0_entry_lo2); }

  static void page_mask(Mword v)
  { Mips::mtc0_32(v, Mips::Cp0_page_mask); }

  static void page_grain(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_page_grain); }

  static void pw_base(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_base); }

  static void pw_field(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_field); }

  static void pw_size(Mword v)
  { Mips::mtc0(v, Mips::Cp0_pw_size); }

  static void wired(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_wired); }

  static Unsigned32 wired()
  { return Mips::mfc0_32(Mips::Cp0_wired); }

  static void pw_ctl(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_pw_ctl); }

  static Unsigned32 pw_ctl()
  { return Mips::mfc0_32(Mips::Cp0_pw_ctl); }

  static void entry_hi(Mword v)
  { Mips::mtc0(v, Mips::Cp0_entry_hi); }

  static Mword entry_hi()
  { return Mips::mfc0(Mips::Cp0_entry_hi); }

  static void make_coherent_to_pou(void const *start, size_t size)
  {
    // Unfortunately 'synci_step' is not available on certain processors
    for (Unsigned8 const *m = (Unsigned8 const*)start;
         m < (Unsigned8 const *)start + size; m += sizeof(Mword))
      Mips::synci(m);
  }

  // Inline wrappers dispatching to the runtime-selected callbacks
  static void tlb_flush(long asid, unsigned guest_id)
  { _tlb_flush(asid, guest_id); }

  static void tlb_flush()
  { _tlb_flush_full(); }

  static void vz_guest_tlb_flush(unsigned guest_id)
  { _vz_guest_tlb_flush(guest_id); }

  static Mword vz_guest_ctl1()
  { return Mips::mfc0_32(Mips::Cp0_guest_ctl_1); }

  static void set_vz_guest_rid(Mword ctl1_orig, Mword guest_id)
  { set_vz_guest_ctl1((ctl1_orig & ~0x00ff0000UL) | ((guest_id & 0x00ff) << 16)); }

  static Mword unique_hi(Mword idx, Mword asid)
  { return (idx << 13) | 0xa0000000 | asid; }

  static bool is_unique_hi(Mword v)
  {
    v >>= 13;
    return v >= (0xa0000000 >> 13) && v < (0xc0000000 >> 13);
  }

  static void set_current_asid(unsigned long asid)
  { entry_hi(asid); }

  // Non-inline public functions — defined in mem_unit-mips.cc
  static void cache_detect(unsigned cm_l2_cache_line);
  static void dcache_flush(Address start, Address end);
  static void dcache_inv(Address start, Address end);
  static void dcache_clean(Address start, Address end);
  static Signed32 tlb_probe();
  static void init_tlb();

private:
  static void set_vz_guest_ctl1(Mword ctl1)
  { Mips::mtc0_32(ctl1, Mips::Cp0_guest_ctl_1); }

  // Private non-inline helpers — defined in mem_unit-mips.cc
  static void tlb_write(Mword v_entry_hi, Mword v_entry_lo0,
                        Mword v_entry_lo1, Mword v_page_mask);
  static Mword tlb_read(Unsigned32 index);

  static void _plain_tlb_flush(long asid, unsigned guest_id);
  static void _plain_tlb_flush_full();
  static void _vz_tlb_flush(long asid, unsigned guest_id);
  static void _vz_tlb_flush_full();
  static void _vz_guest_tlb_flush_impl(unsigned guest_id);
  static void _vz_tlbinv_tlb_flush(long asid, unsigned guest_id);
  static void _vz_tlbinv_tlb_flush_full();
  static void _vz_guest_tlbinv_tlb_flush_impl(unsigned guest_id);
  static void _vz_tlbinv_ftlb_flush_loop(long asid, unsigned guest_id);
  static void _vz_tlbinv_ftlb_flush_loop_full();
  static void _tlbinv_ftlb_flush_loop(long asid, unsigned guest_id);
  static void _tlbinv_ftlb_flush_loop_full();
  static void _tlbinv_tlb_flush(long asid, unsigned guest_id);
  static void _tlbinv_tlb_flush_full();

  static void (*_tlb_flush)(long asid, unsigned guest_id);
  static void (*_tlb_flush_full)();
  static void (*_vz_guest_tlb_flush)(unsigned guest_id);
};
