#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_armada38x : Address
  {
    L2cxx0_phys_base     = 0xf1008000,
    Gic_cpu_phys_base    = 0xf100c100,
    Gic_dist_phys_base   = 0xf100d000,
    Mp_scu_phys_base     = 0xf100c000,
  };
};
