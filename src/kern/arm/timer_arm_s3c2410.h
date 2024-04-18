#pragma once

#include <types.h>
#include <mmio_register_block.h>

class Timer_arm_s3c2410 : private Mmio_register_block
{
protected:
  enum {
    TCFG0      = 0x00,
    TCFG1      = 0x04,
    TCON       = 0x08,
    TCNTB0     = 0x0c,
    TCMPB0     = 0x10,
    TCNTO0     = 0x14,
    TCNTB1     = 0x18,
    TCMPB1     = 0x1c,
    TCNTO1     = 0x20,
    TCNTB2     = 0x24,
    TCMPB2     = 0x28,
    TCNTO2     = 0x2c,
    TCNTB3     = 0x30,
    TCMPB3     = 0x34,
    TCNTO3     = 0x38,
    TCNTB4     = 0x3c,
    TCNTO4     = 0x40,
    TINT_CSTAT = 0x44,
  };

  enum {
    Timer_nr = 4,
  };

  static Static_object<Timer_arm_s3c2410> _timer;

public:
  explicit Timer_arm_s3c2410(Address phys_base, bool tint_cstat, Mword reload_value);

  static void acknowledge_cint()
  {
    _timer->modify<Mword>(1 << (Timer_nr + 5), 0, TINT_CSTAT);
  }
};
