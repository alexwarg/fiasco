#pragma once

#include <mmio_register_block.h>
#include <mem_layout.h>
#include <irq_chip.h>

class Timer_arm_imx_epit : private Mmio_register_block
{
public:
  void acknowledge()
  {
    write<Mword>(EPITSR_OCIF, EPITSR);
  }

  explicit Timer_arm_imx_epit(Address phys_base);

private:
  enum {
    EPITCR   = 0x00,
    EPITSR   = 0x04,
    EPITLR   = 0x08,
    EPITCMPR = 0x0c,
    EPITCNR  = 0x10,

    EPITCR_ENABLE                  = 1 << 0, // enable EPIT
    EPITCR_ENMOD                   = 1 << 1, // enable mode
    EPITCR_OCIEN                   = 1 << 2, // output compare irq enable
    EPITCR_RLD                     = 1 << 3, // reload
    EPITCR_SWR                     = 1 << 16, // software reset
    EPITCR_WAITEN                  = 1 << 19, // wait enabled
    EPITCR_CLKSRC_IPG_CLK          = 1 << 24,
    EPITCR_CLKSRC_IPG_CLK_HIGHFREQ = 2 << 24,
    EPITCR_CLKSRC_IPG_CLK_32K      = 3 << 24,
    EPITCR_PRESCALER_SHIFT         = 4,
    EPITCR_PRESCALER_MASK          = ((1 << 12) - 1) << EPITCR_PRESCALER_SHIFT,

    EPITSR_OCIF = 1,
  };
};
