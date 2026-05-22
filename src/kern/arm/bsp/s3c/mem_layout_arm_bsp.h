#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_s3c2410 : Address {
    Pic_phys_base        = 0x4a000000,
    Pwm_phys_base        = 0x51000000,
    Watchdog_phys_base   = 0x53000000,
  };
};
