#pragma once

#include <types.h>
#include <config.h>
#include <irq_chip.h>

class Timer_pit
{
public:
  static void init(Cpu_number);

  static int irq()
  { return 0; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }


  static void acknowledge()
  {}

  static void update_timer(Unsigned64)
  {}
};
