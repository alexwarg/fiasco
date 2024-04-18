#pragma once

#include <mmio_register_block.h>
#include <irq_chip.h>
#include <types.h>

class Timer_arm_rpi : private Mmio_register_block
{
public:
  static unsigned irq() { return 3; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  Timer_arm_rpi(Address base) : Mmio_register_block(base)
  {
    set_next();
  }

  static void init(Cpu_number cpu);

  static void acknowledge()
  {
    _timer->set_next();
    _timer->write<Mword>(1 << Timer_nr, CS);
  }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {
    static_assert(!Config::Scheduler_one_shot,
                  "currently no dynamic ticks with ARM generic timer");
  }

private:
  enum
  {
    CS  = 0,
    CLO = 4,
    CHI = 8,
    C0  = 0xc,
    C1  = 0x10,
    C2  = 0x14,
    C3  = 0x18,

    Timer_nr = 3,
    Interval = 1000,
  };

  static Static_object<Timer_arm_rpi> _timer;

  void set_next()
  {
    write<Mword>(read<Mword>(CLO) + Interval, C0 + Timer_nr * 4);
  }

};


