#pragma once

#include <irq_chip.h>
#include <types.h>
#include <mmio_register_block.h>

class Timer_integrator : private Mmio_register_block
{
public:
  static unsigned irq() { return 6; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void init(Cpu_number);

  static void acknowledge()
  {
    _timer->write(1u, TIMER1_BASE + TIMER_INTCLR);
  }

  explicit Timer_integrator(Address base);

private:
  enum {
    TIMER0_BASE = 0x000,
    TIMER1_BASE = 0x100,
    TIMER2_BASE = 0x200,

    TIMER_LOAD   = 0x00,
    TIMER_VALUE  = 0x04,
    TIMER_CTRL   = 0x08,
    TIMER_INTCLR = 0x0c,

    TIMER_CTRL_IE       = 1 << 5,
    TIMER_CTRL_PERIODIC = 1 << 6,
    TIMER_CTRL_ENABLE   = 1 << 7,
  };

  static Static_object<Timer_integrator> _timer;
};
