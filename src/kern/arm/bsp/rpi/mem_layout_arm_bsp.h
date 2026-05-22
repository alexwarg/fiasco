#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
#if defined(CONFIG_PF_RPI_RPI1) || defined(CONFIG_PF_RPI_RPIZW)
  enum Phys_layout_bcm2835 : Address {
    Bcm283x_base = 0x20000000,
  };
#endif

#if defined(CONFIG_PF_RPI_RPI2) || defined(CONFIG_PF_RPI_RPI3)
  enum Phys_layout_bcm2836_7 : Address {
    Bcm283x_base = 0x3f000000,
  };
#endif

#if defined(CONFIG_PF_RPI_RPI1) || defined(CONFIG_PF_RPI_RPIZW) \
    || defined(CONFIG_PF_RPI_RPI2) || defined(CONFIG_PF_RPI_RPI3)
  enum Phys_layout_bcm283x : Address {
    Local_intc           = 0x40000000,
    Pic_phys_base        = Bcm283x_base + 0x0000b200,
    Timer_phys_base      = Bcm283x_base + 0x00003000,
    Watchdog_phys_base   = Bcm283x_base + 0x00100000,
  };
#endif

#ifdef CONFIG_PF_RPI_RPI4
  enum Phys_layout_bcm2711 : Address {
    Bcm2711_base       = 0xfe000000,
    Local_intc         = 0xff800000,
    Watchdog_phys_base = Bcm2711_base + 0x00100000,
  };
#endif
};
