#pragma once

#include "irq_chip.h"
#include "mmio_register_block.h"
#include "l4_types.h"

class Gic_redist
{
private:
  Mmio_register_block _redist;

public:
  enum
  {
    GICR_CTRL         = 0x0000,
    GICR_IIDR         = 0x0004,
    GICR_TYPER        = 0x0008,
    GICR_STATUSR      = 0x0010,
    GICR_WAKER        = 0x0014,
    GICR_PIDR2        = 0xffe8,
    GICR_SGI_BASE     = 0x10000,
    GICR_IGROUPR0     = GICR_SGI_BASE + 0x0080,
    GICR_ISENABLER0   = GICR_SGI_BASE + 0x0100,
    GICR_ICENABLER0   = GICR_SGI_BASE + 0x0180,
    GICR_ISPENDR0     = GICR_SGI_BASE + 0x0200,
    GICR_ICPENDR0     = GICR_SGI_BASE + 0x0280,
    GICR_ISACTIVER0   = GICR_SGI_BASE + 0x0300,
    GICR_ICACTIVER0   = GICR_SGI_BASE + 0x0380,
    GICR_IPRIORITYR0  = GICR_SGI_BASE + 0x0400,
    GICR_ICFGR0       = GICR_SGI_BASE + 0x0c00,

    GICR_frame_size   = 0x10000,

    GICR_TYPER_VLPIS  = 1 << 1,
    GICR_TYPER_Last   = 1 << 4,

    GICR_WAKER_Processor_sleep = 1 << 1,
    GICR_WAKER_Children_asleep = 1 << 2,
  };

  void find(Address base, Unsigned64 mpidr, Cpu_number cpu);
  void cpu_init();

  void mask(Mword pin)
  {
    _redist.write<Unsigned32>(1u << pin, GICR_ICENABLER0);
  }

  void unmask(Mword pin)
  {
    _redist.write<Unsigned32>(1u << pin, GICR_ISENABLER0);
  }

  void irq_prio(unsigned irq, unsigned prio)
  {
    _redist.write<Unsigned8>(prio, GICR_IPRIORITYR0 + irq);
  }

  unsigned irq_prio(unsigned irq)
  {
    return _redist.read<Unsigned8>(GICR_IPRIORITYR0 + irq);
  }

  int set_mode(Mword pin, Irq_chip::Mode m)
  {
    if (!m.set_mode())
      return 0;

    unsigned v = 0;
    switch (m.flow_type())
      {
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high:
        break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high:
        v = 2;
        break;
      default:
        return -L4_err::EInval;
      };

    unsigned shift = (pin & 15) * 2;

    _redist.modify<Unsigned32>(v << shift, 3 << shift, GICR_ICFGR0 + (pin >> 4) * 4);

    return 0;
  }
};
