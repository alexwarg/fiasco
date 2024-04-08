
#include "gic_redist.h"

#include "cpu.h"
#include "l4_types.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include <cstdio>

void
Gic_redist::find(Address base, Unsigned64 mpidr, Cpu_number cpu)
{
  unsigned o = 0;
  Unsigned64 gicr_typer;
  Unsigned64 typer_aff =   ((mpidr & 0x0000ffffff) << (32 - 0))
                         | ((mpidr & 0xff00000000) << (56 - 32));
  do
    {
      Mmio_register_block r(base + o);

      unsigned arch_rev = (r.read<Unsigned32>(GICR_PIDR2) >> 4) & 0xf;
      if (arch_rev != 0x3 && arch_rev != 0x4)
        // No GICv3 and no GICv4
        break;

      gicr_typer = r.read<Unsigned64>(GICR_TYPER);
      if ((gicr_typer & 0xffffffff00000000) == typer_aff)
        {
          printf("CPU%d: GIC Redistributor at %lx for 0x%llx\n",
                 cxx::int_value<Cpu_number>(cpu),
                 r.get_mmio_base(), mpidr & ~0xc0000000ull);
          _redist = r;
          return;
        }

      o += 2 * GICR_frame_size;
      if (gicr_typer & GICR_TYPER_VLPIS)
        o += 2 * GICR_frame_size;
    }
  while (!(gicr_typer & GICR_TYPER_Last));

  panic("GIC: Did not find a redistributor for CPU%d\n",
        cxx::int_value<Cpu_number>(cpu));
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

