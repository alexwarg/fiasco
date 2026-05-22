#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_zynq : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    L2cxx0_phys_base     = 0xf8f02000,
  };
};
