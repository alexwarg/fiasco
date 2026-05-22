#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_integrator : Address {
    Integrator_phys_base = 0x10000000,
    Timer_phys_base      = 0x13000000,
    Pic_phys_base        = 0x14000000,
  };
};
