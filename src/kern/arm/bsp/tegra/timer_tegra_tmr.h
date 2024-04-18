#pragma once

#include <irq_chip.h>
#include <mmio_register_block.h>
#include <types.h>

class Timer_tegra_tmr
{
public:
  static unsigned irq() { return 32; };

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void init(Cpu_number);

  static void acknowledge()
  {
    _tmr->write<Mword>(1 << 30, Reg::PCR);
  }

private:
  static Static_object<Mmio_register_block> _tmr;

  struct Reg { enum
  {
    PTV = 0,
    PCR = 4,
  }; };

};
