#pragma once

#include <irq_chip.h>
#include <timer_omap_1mstimer.h>

class Timer_omap3
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
  static Static_object<Timer_omap_1mstimer> _timer;

  static void init_am33xx(Address wkup_phys, Address clksel_phys);
  static void init_35xx(Address wkup_phys);
};

