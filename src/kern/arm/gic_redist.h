#pragma once

#include "irq_chip.h"
#include "mmio_register_block.h"
#include <poll_timeout_counter.h>
#include <warn.h>
#include <processor.h>
#include "l4_types.h"
#include <cxx/bitfield>

#include <globalconfig.h>
#ifdef CONFIG_ARM_GIC_MSI
#include <gic_mem.h>
#endif

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
    GICR_PROPBASER    = 0x0070,
    GICR_PENDBASER    = 0x0078,
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

    GICR_WAKER_Processor_sleep = 1 << 1,
    GICR_WAKER_Children_asleep = 1 << 2,
  };

  struct Typer
  {
    Unsigned64 raw;
    Typer() = default;
    explicit Typer(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 0,  0, plpis, raw);
    CXX_BITFIELD_MEMBER_RO( 1,  1, vlpis, raw);
    CXX_BITFIELD_MEMBER_RO( 4,  4, last, raw);
    CXX_BITFIELD_MEMBER_RO(32, 63, affinity, raw);
  };

  void find(Address base, Unsigned64 mpidr, Cpu_number cpu);
  void cpu_init();

  void mask(Mword pin)
  {
    _redist.write<Unsigned32>(1u << pin, GICR_ICENABLER0);
    sync_rwp();
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

#ifdef CONFIG_ARM_GIC_MSI
  static void init_lpi(unsigned num_lpis);
  void cpu_init_lpi();

  static void enable_lpi(Mword lpi, bool enabled)
  {
    Unsigned8 *lpi_config = lpi_config_table.virt_ptr<Unsigned8>() + lpi;
    write_now(lpi_config, GICR_lpi_default_prio | enabled);
    lpi_config_table.make_coherent(lpi_config, lpi_config + 1);
  }

  Address get_base() const
  {
    return _redist.get_mmio_base();
  }
#endif

private:
  void sync_rwp()
  {
    L4::Poll_timeout_counter i(1U << 27); // ~134ms @ 1GHz
    while (i.test(_redist.read<Unsigned32>(GICR_CTRL) & (1u << 3)))
      Proc::pause();

    if (EXPECT_FALSE(i.timed_out()))
      WARNX(Error, "GICR: RWP timed out!\n");
  }

  enum
  {
    GICR_lpi_default_prio = 0xa0,

    GICR_config_table_align  = 0x1000,
    GICR_pending_table_align = 0x10000,
  };

  struct Ctrl
  {
    Unsigned32 raw;
    Ctrl() = default;
    explicit Ctrl(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  0, enable_lpis, raw);
  };

  struct Propbaser
  {
    Unsigned64 raw;
    Propbaser() = default;
    explicit Propbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  4, id_bits, raw);
    CXX_BITFIELD_MEMBER          ( 7,  9, cacheability, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
  };

  struct Pendbaser
  {
    Unsigned64 raw;
    Pendbaser() = default;
    explicit Pendbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 7,  9, cacheability, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (62, 62, ptz, raw);
  };

#ifdef CONFIG_ARM_GIC_MSI
  Gic_mem::Mem_chunk _lpi_pending_table;

  static unsigned num_lpi_intid_bits;
  static Gic_mem::Mem_chunk lpi_config_table;
#endif
};
