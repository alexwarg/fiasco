#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_sunxi : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    Timer_phys_base      = 0x01c20c00,
  };
};
