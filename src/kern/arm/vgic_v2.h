
#pragma once

#include "types.h"
#include "mem.h"
#include "kmem.h"
#include "mmio_register_block.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v2 :
  public Gic_h_mixin<Gic_h_v2>,
  private Mmio_register_block
{
public:
  enum { Version = 2 };
  enum Register
  {
    HCR   = 0x000,
    VTR   = 0x004,
    VMCR  = 0x008,
    MISR  = 0x010,

    EISRn = 0x020,
    EISR0 = 0x020,
    EISR1 = 0x024,

    ELSRn = 0x030,
    ELSR0 = 0x030,
    ELSR1 = 0x034,

    APR   = 0x0f0,

    LRn   = 0x100,
    LR0   = LRn,
    LR63  = 0x1fc
  };

  struct Lr
  {
    Unsigned32 raw;
    Lr() = default;
    explicit Lr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  9, vid, raw);
    CXX_BITFIELD_MEMBER( 10, 19, pid, raw);
    CXX_BITFIELD_MEMBER( 10, 12, cpuid, raw);
    CXX_BITFIELD_MEMBER( 19, 19, eoi, raw);
    CXX_BITFIELD_MEMBER( 23, 27, prio, raw);
    CXX_BITFIELD_MEMBER( 28, 29, state, raw);
    CXX_BITFIELD_MEMBER( 30, 30, grp1, raw);
    CXX_BITFIELD_MEMBER( 31, 31, hw, raw);
  };

  explicit Gic_h_v2(Address va) : Mmio_register_block(va) {}

  static Static_object<Gic_h_v2> gic;

  Address gic_v_address() const override
  { return Mem_layout::Gic_v_phys_base; }

  Hcr hcr() const
  { return Hcr(read<Unsigned32>(HCR)); }

  void hcr(Hcr hcr)
  { write(hcr.raw, HCR); }

  Vtr vtr() const
  { return Vtr(read<Unsigned32>(VTR)); }

  Vmcr vmcr() const
  { return Vmcr(read<Unsigned32>(VMCR)); }

  void vmcr(Vmcr vmcr)
  { write(vmcr.raw, VMCR); }

  Misr misr() const
  { return Misr(read<Unsigned32>(MISR)); }

  Unsigned32 eisr() const
  { return read<Unsigned32>(EISRn); }

  Unsigned32 elsr() const
  { return read<Unsigned32>(ELSRn); }

  void save_aprs(Unsigned32 *a) const
  { a[0] = read<Unsigned32>(APR); }

  void load_aprs(Unsigned32 const *a)
  { write(a[0], APR); }

  void save_lrs(Gic_h::Arm_vgic::Lrs *l, unsigned n) const
  {
    for (unsigned i = 0; i < n; ++i)
      l->lr32[i] = read<Unsigned32>(LRn + (i << 2));
  }

  void load_lrs(Gic_h::Arm_vgic::Lrs const *l, unsigned n)
  {
    for (unsigned i = 0; i < n; ++i)
      write(l->lr32[i], LRn + (i << 2));
  }

  static void vgic_barrier()
  { Mem::dsb(); /* Ensure vgic completion before running user-land */ }

};

