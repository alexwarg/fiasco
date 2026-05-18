#pragma once

#include <types.h>
#include <config.h>
#include <irq_chip.h>
#include <rtc-ia32.h>

class Timer_rtc
{
public:
  static void init(Cpu_number);

  static int irq()
  { return 8; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }


  static void acknowledge()
  {
    Rtc::reset();
  }

  static void update_timer(Unsigned64)
  {}
};
