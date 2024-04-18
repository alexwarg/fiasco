#pragma once

#include <irq_chip.h>
#include <tcu_jz4780.h>
#include <static_init.h>

class Timer
{
public:
  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_level_high; }

  static unsigned irq()
  { return 27; }

  static void init(Cpu_number cpu);
  static void enable();

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void acknowledge()
  {
    _tcu->r[Tcu_jz4780::TFCR] = 1 << 15;
  }

  static Unsigned64 get_current_counter()
  {
    Unsigned32 lo = _tcu->r[Tcu_jz4780::OSTCNTL];
    Unsigned32 hi = _tcu->r[Tcu_jz4780::OSTCNTH];

    return (((Unsigned64)hi) << 32) | lo;
  }

private:
  static Static_object<Tcu_jz4780> _tcu;

};
