
#include <timer_omap_gentimer.h>

#include <kmem.h>
#include <mem_layout.h>

Timer_omap_gentimer::Timer_omap_gentimer()
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timergen_phys_base, 0x100))
{
  // Mword idr = Io::read<Mword>(TIDR);
  // older timer: idr >> 16 == 0
  // newer timer: idr >> 16 != 0

  // reset
  write<Mword>(1, TIOCP_CFG);
  while (read<Mword>(TIOCP_CFG) & 1)
    ;
  // reset done

  // overflow mode
  write<Mword>(2, IRQENABLE_SET);
  // no wakeup
  write<Mword>(0, IRQWAKEEN);

  // program 1000 Hz timer frequency
  // (FFFFFFFFh - TLDR + 1) * timer-clock-period * clock-divider(ps)
  Mword val = 0xffffffda;
  write<Mword>(val, TLDR);
  write<Mword>(val, TCRR);

  write<Mword>(1 | 2, TCLR);
}

