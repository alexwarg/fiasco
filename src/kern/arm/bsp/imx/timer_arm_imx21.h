#pragma once

#include <irq_chip.h>
#include <mmio_register_block.h>

class Timer_arm_imx21 : private Mmio_register_block
{
public:
  Timer_arm_imx21(Address phys_base, unsigned long size);

private:
  enum {
    TCTL  = 0x00,
    TPRER = 0x04,
    TCMP  = 0x08,
    TCR   = 0x0c,
    TCN   = 0x10,
    TSTAT = 0x14,

    TCTL_TEN                            = 1 << 0,
    TCTL_CLKSOURCE_PERCLK1_TO_PRESCALER = 1 << 1,
    TCTL_CLKSOURCE_32kHz                = 1 << 3,
    TCTL_COMP_EN                        = 1 << 4,
    TCTL_SW_RESET                       = 1 << 15,
  };

public:
  void acknowledge() const { write<Mword>(1, TSTAT); }
};

