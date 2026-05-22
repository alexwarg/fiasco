#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_sa1100 : Address {
    Timer_phys_base      = 0x90000000,
    Pic_phys_base        = 0x90050000,
    Flush_area_phys_base = 0xe0000000,
  };
};
