#pragma once

#include <mem_layout_arm_bsp.h>
#include <kmem_mmio.h>

namespace Outer_cache
{
  inline void platform_init_post()
  {}

  static Mword platform_init(Mword aux_control)
  {
    l2cxx0.construct(Kmem_mmio::map(Mem_layout_arm_bsp::L2cxx0_phys_base, 0x1000));
    return l2cxx0->read<Unsigned32>(L2cxx0::AUX_CONTROL);
  }
}
