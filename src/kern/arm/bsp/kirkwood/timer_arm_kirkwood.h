#pragma once

#include <mmio_register_block.h>
#include <types.h>
#include <irq_chip.h>

class Timer_arm_kirkwood : private Mmio_register_block
{
public:
  static unsigned irq() { return 1; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  Timer_arm_kirkwood();

  static void init(Cpu_number)
  {
    _timer.construct();
  }

  static void acknowledge()
  {
    _timer->modify<Unsigned32>(0, Timer0_bridge_num, Bridge_cause);
  }

  static void update_timer(Unsigned64 /*wkaueup*/)
  {}

private:
  enum {
    Control_Reg  = 0x300,
    Reload0_Reg  = 0x310,
    Timer0_Reg   = 0x314,
    Reload1_Reg  = 0x318,
    Timer1_Reg   = 0x31c,

    Bridge_cause = 0x110,
    Bridge_mask  = 0x114,

    Timer0_enable = 1 << 0,
    Timer0_auto   = 1 << 1,

    Timer0_bridge_num = 1 << 1,
    Timer1_bridge_num = 1 << 2,

    Reload_value = 200000,
  };

  static Static_object<Timer_arm_kirkwood> _timer;
};

