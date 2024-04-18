#pragma once

#include <types.h>
#include <irq_chip.h>

class Timer_hpet
{
public:
  static void init(Cpu_number);

  static int irq()
  { return hpet_irq; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_level_low; }


  static void acknowledge()
  {}

  static void update_timer(Unsigned64)
  {}

private:
  static int hpet_irq;
};
