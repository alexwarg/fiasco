#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_kirkwood : Address
  {
    Reset_phys_base   = 0xf1020000,
    Timer_phys_base   = 0xf1020000,
    Pic_phys_base     = 0xf1020000,
  };
};
