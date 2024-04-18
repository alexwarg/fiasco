#pragma once

#include <timer_omap_gentimer.h>
#include <irq_chip.h>

class Timer_omap3_am33xx
{
public:
  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void acknowledge()
  {
    _timer->acknowledge();
  }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

protected:
  static Static_object<Timer_omap_gentimer> _timer;

  static void init_timer(Address wkup_phys);
};

