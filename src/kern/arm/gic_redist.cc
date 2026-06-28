
#include "gic_redist.h"

#include "cpu.h"
#include "l4_types.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include <cstdio>
#include <cstring>
#include <arithmetic.h>

Mmio_register_block
Gic_redist_find::scan_range(Address base, Unsigned64 mpidr)
{
  unsigned o = 0;
  Gic_redist::Typer gicr_typer;
  Unsigned64 typer_aff = (mpidr & 0x0000ffffff) | ((mpidr & 0xff00000000) >> 8);
  do
    {
      Mmio_register_block r(base + o);

      unsigned arch_rev = (r.read<Unsigned32>(Gic_redist::GICR_PIDR2) >> 4) & 0xf;
      if (arch_rev != 0x3 && arch_rev != 0x4)
        // No GICv3 and no GICv4
        break;

      gicr_typer.raw = r.read_non_atomic<Unsigned64>(Gic_redist::GICR_TYPER);
      if (gicr_typer.affinity() == typer_aff)
        return r;

      o += 2 * Gic_redist::GICR_frame_size;
      if (gicr_typer.vlpis())
        o += 2 * Gic_redist::GICR_frame_size;
    }
  while (!gicr_typer.last());

  return Mmio_register_block(nullptr);
}

void
Gic_redist::cpu_init()
{
  unsigned val = _redist.read<Unsigned32>(GICR_WAKER);
  if (val & GICR_WAKER_Children_asleep)
    {
      val &= ~GICR_WAKER_Processor_sleep;
      _redist.write<Unsigned32>(val, GICR_WAKER);

      L4::Poll_timeout_counter i(5000000);
      while (i.test(_redist.read<Unsigned32>(GICR_WAKER) & GICR_WAKER_Children_asleep))
        Proc::pause();

      if (i.timed_out())
        panic("GIC: redistributor did not awake\n");
    }

  _redist.write<Unsigned32>(0xffffffff, GICR_ICENABLER0);

  _redist.write<Unsigned32>(0x0000001e, GICR_ISENABLER0);
  _redist.write<Unsigned32>(0xffffffff, GICR_IGROUPR0);

  _redist.write<Unsigned32>(0xffffffff, GICR_ICPENDR0);
  _redist.write<Unsigned32>(0xffffffff, GICR_ICACTIVER0); // clear active

  for (unsigned g = 0; g < 32; g += 4)
    _redist.write<Unsigned32>(0xa0a0a0a0, GICR_IPRIORITYR0 + g);
}

void
Gic_redist::disable()
{
  unsigned val = _redist.read<Unsigned32>(GICR_WAKER);
  val |= GICR_WAKER_Processor_sleep;
  _redist.write<Unsigned32>(val, GICR_WAKER);

  L4::Poll_timeout_counter i(5000000);
  while (i.test(!(_redist.read<Unsigned32>(GICR_WAKER) & GICR_WAKER_Children_asleep)))
    Proc::pause();

  if (i.timed_out())
    panic("GIC: redistributor still active\n");
}

#ifdef CONFIG_ARM_GIC_MSI

#include <gic_dist.h>

unsigned Gic_redist::num_lpi_intid_bits;
Gic_mem Gic_redist::lpi_config_table;

void
Gic_redist::init_lpi(unsigned num_lpis)
{
  num_lpi_intid_bits = cxx::log2u(Gic_dist::Lpi_intid_base + num_lpis - 1) + 1;
  num_lpis = (1U << num_lpi_intid_bits) - Gic_dist::Lpi_intid_base;

  lpi_config_table = Gic_mem::alloc_mem(num_lpis, GICR_config_table_align);
  if (!lpi_config_table.is_valid())
    panic("GIC: Failed to allocate redistributor LPI configuration table.\n");
  // Initialize all LPIs with default priority and disabled.
  memset(lpi_config_table.virt_ptr(), GICR_lpi_default_prio, num_lpis);
}

void
Gic_redist::cpu_init_lpi()
{
  Typer gicr_typer(_redist.read_non_atomic<Unsigned64>(GICR_TYPER));
  if (!gicr_typer.plpis())
    panic("GIC: Redistributor does not support physical LPIs.\n");

  Ctrl ctrl(_redist.read<Unsigned32>(GICR_CTRL));
  if (ctrl.enable_lpis())
    panic("GIC: LPI support of redistributor is already enabled.\n");

  Propbaser propbaser;
  propbaser.id_bits() = num_lpi_intid_bits - 1;
  propbaser.pa() = lpi_config_table.phys_addr();
  lpi_config_table.setup_reg(_redist.r<Unsigned64>(GICR_PROPBASER), propbaser);
  lpi_config_table.make_coherent();

  // Each bit in the pending table represents the pending state of one LPI.
  // The first 1 KiB is reserved for the pending state of SGIs/PPIs/SPIs.
  unsigned lpi_pending_table_size = (1U << num_lpi_intid_bits) / 8;
  // Zero initialize pending table, no LPIs are pending.
  _lpi_pending_table = Gic_mem::alloc_zmem(lpi_pending_table_size,
                                           GICR_pending_table_align);
  if (!_lpi_pending_table.is_valid())
    panic("GIC: Failed to allocate redistributor LPI pending table.\n");

  Pendbaser pendbaser;
  pendbaser.pa() = _lpi_pending_table.phys_addr();
  pendbaser.ptz() = 1;
  _lpi_pending_table.setup_reg(_redist.r<Unsigned64>(GICR_PENDBASER), pendbaser);
  _lpi_pending_table.make_coherent();

  // Enable LPI support for redistributor.
  ctrl.enable_lpis() = 1;
  _redist.write<Unsigned32>(ctrl.raw, GICR_CTRL);
}

#endif
