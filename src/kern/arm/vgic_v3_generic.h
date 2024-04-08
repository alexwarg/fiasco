
#pragma once

#include "types.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v3 : public Gic_h_mixin<Gic_h_v3>
{
public:
  enum { Version = 3 };

  Gic_h_v3() : n_aprs(1U << (vtr().pri_bits() - 4))
  {}

  Address gic_v_address() const override { return 0; }

  // fake pointer to call static functions
  static Gic_h const *const gic;
  unsigned n_aprs;

  static void vgic_barrier()
  {
    // eret has implicit isb semantics, so no need here to ensure vgic
    // completion before running user-land
  }

  static Hcr hcr();
  static void hcr(Hcr hcr);
  static Vtr vtr();
  static Vmcr vmcr();
  static void vmcr(Vmcr vmcr);
  static Misr misr();
  static Unsigned32 eisr();
  static inline Unsigned32 elsr();
  void save_aprs(Unsigned32 *a);
  void load_aprs(Unsigned32 const *a);
  static void save_lrs(Gic_h::Arm_vgic::Lrs *lr, unsigned n);
  static void load_lrs(Gic_h::Arm_vgic::Lrs const *lr, unsigned n);
};

