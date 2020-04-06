#pragma once

#include <cassert>
#include <cstdio>

#include "cascade_irq.h"
#include "gic_iface.h"
#include "irq_entry.h"
#include "mmio_register_block.h"
#include "std_macros.h"

/**
 * Driver for the Freescale/NXP i.MX TZIC (TrustZone Interrupt Controller).
 *
 * Used on i.MX51 and i.MX53. Single unified register block, no separate CPU
 * interface. Supports 128 interrupt sources, level-triggered only, single-core.
 */
class Gic_tzic : public Gic
{
public:
  IRQ_CHIP_DBG_INFO("TZIC");

  enum : unsigned
  {
    TZIC_INTCTRL  = 0x000,
    TZIC_INTTYPE  = 0x004,
    TZIC_PRIOMASK = 0x00c,
    TZIC_SYNCCTRL = 0x010,
    TZIC_INTSEC   = 0x080, // [0..3] × 4 bytes = 128 IRQs
    TZIC_ENSET    = 0x100, // [0..3]
    TZIC_ENCLEAR  = 0x180, // [0..3]
    TZIC_SRCSET   = 0x200, // [0..3]
    TZIC_SRCCLEAR = 0x280, // [0..3]
    TZIC_PRIORITY = 0x400, // 128 bytes, one per IRQ
    TZIC_PND      = 0xd00, // [0..3]

    TZIC_INTCTRL_ENABLE   = 1u << 0,
    TZIC_INTCTRL_NSEN     = 1u << 16,
    TZIC_INTCTRL_NSENMASK = 1u << 31,

    Num_irqs       = 128,
    Default_prio   = 0xa0,
    Cpu_prio_val   = 0xf0,
  };

private:
  Mmio_register_block _regs;

  unsigned pending()
  {
    for (unsigned g = 0; g < Num_irqs; g += 32)
      {
        Unsigned32 v = _regs.read<Unsigned32>(TZIC_PND + (g / 8));
        if (v)
          return g + __builtin_ctz(v);
      }
    return ~0u;
  }

public:
  explicit Gic_tzic(Address base, int num_irqs_override = -1)
    : _regs(base)
  {
    init_tzic(num_irqs_override);
  }

  void init_tzic(int num_override = -1)
  {
    unsigned num = (num_override > 0) ? (unsigned)num_override : Num_irqs;

    for (unsigned g = 0; g < num; g += 32)
      {
        unsigned off = (g / 32) * 4;
        _regs.write<Unsigned32>(~0u, TZIC_ENCLEAR  + off);
        _regs.write<Unsigned32>(~0u, TZIC_SRCCLEAR + off);
        _regs.write<Unsigned32>(~0u, TZIC_INTSEC   + off); // all non-secure
      }

    for (unsigned i = 0; i < num; ++i)
      _regs.write<Unsigned8>(Default_prio, TZIC_PRIORITY + i);

    _regs.write<Unsigned32>(0, TZIC_SYNCCTRL);
    _regs.write<Unsigned32>(Cpu_prio_val, TZIC_PRIOMASK);
    _regs.write<Unsigned32>(
      TZIC_INTCTRL_ENABLE | TZIC_INTCTRL_NSEN | TZIC_INTCTRL_NSENMASK,
      TZIC_INTCTRL);

    Irq_chip_gen::init(num);
    printf("TZIC: %u IRQs\n", num);
  }

  // --- Irq_chip ---

  void mask(Mword pin) override
  {
    assert(cpu_lock.test());
    _regs.write<Unsigned32>(1u << (pin & 31), TZIC_ENCLEAR + (pin / 32) * 4);
  }

  void unmask(Mword pin) override
  {
    assert(cpu_lock.test());
    _regs.write<Unsigned32>(1u << (pin & 31), TZIC_ENSET + (pin / 32) * 4);
  }

  void mask_and_ack(Mword pin) override
  {
    assert(cpu_lock.test());
    _regs.write<Unsigned32>(1u << (pin & 31), TZIC_ENCLEAR + (pin / 32) * 4);
  }

  void ack(Mword) override {}

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}

  // --- Gic ---

  void softint_cpu(Cpu_number, unsigned) override  {}
  void softint_bcast(unsigned) override            {}
  void softint_phys(unsigned, Unsigned64) override {}

  void cpu_deinit(Cpu_number) override
  {}

  unsigned gic_version() const override { return 2; }

  void set_pending_irq(unsigned idx, Unsigned32 val) override
  {
    if (idx < 4)
      _regs.write<Unsigned32>(val, TZIC_SRCSET + idx * 4);
  }

  unsigned get_pending() override { return pending(); }

  Hit_func get_cascade_hit() override { return &cascade_hit; }

  // --- IRQ dispatch ---

  void hit(Upstream_irq const *u)
  {
    unsigned num = pending();
    if (EXPECT_FALSE(num == ~0u))
      return;
    handle_irq<Gic_tzic>(num, u);
  }

  static void cascade_hit(Irq_base *_self, Upstream_irq const *u)
  {
    Cascade_irq *self = nonull_static_cast<Cascade_irq *>(_self);
    Gic_tzic *tzic = nonull_static_cast<Gic_tzic *>(self->child());
    Upstream_irq ui(self, u);
    tzic->hit(&ui);
  }

  void set_as_primary_irq_handler()
  {
    Gic::primary = this;
    Arm_irqs::set_irq_handler([]()
      { nonull_static_cast<Gic_tzic *>(Gic::primary)->hit(nullptr); });
  }
};
