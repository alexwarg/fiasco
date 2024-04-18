#pragma once

#include <mmio_register_block.h>

class Timer_imx_timrot
{
public:
  Timer_imx_timrot(Address phys, unsigned size);

private:
  enum
  {
    HW_TIMROT_TIMCTRL0       = 0x20,
    HW_TIMROT_TIMCTRL0_SET   = 0x24,
    HW_TIMROT_TIMCTRL0_CLR   = 0x28,
    HW_TIMROT_RUNNING_COUNT0 = 0x30,
    HW_TIMROT_FIXED_COUNT0   = 0x40,

    CTRL_SELECT_32KHZ = 0xb << 0,
    CTRL_RELOAD       = 1   << 6,
    CTRL_UPDATE       = 1   << 7,
    CTRL_IRQ_EN       = 1   << 14,
    CTRL_IRQ          = 1   << 15,
  };

  Register_block<32> _reg;

public:
  void acknowledge() const { _reg[HW_TIMROT_TIMCTRL0_CLR] = CTRL_IRQ; }
};


