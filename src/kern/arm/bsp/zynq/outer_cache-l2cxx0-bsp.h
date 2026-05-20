#pragma once

#include <mem_layout_arm_bsp.h>
#include <kmem.h>

namespace Outer_cache
{
  inline void platform_init_post()
  {}

  static Mword platform_init()
  {
    l2cxx0.construct(Kmem::mmio_remap(Mem_layout_arm_bsp::L2cxx0_phys_base, 0x1000));
    Mword aux_control = l2cxx0->read<Unsigned32>(L2cxx0::AUX_CONTROL);
    // 64k way, 8-way associativityciativity
    return (aux_control & 0xf2f0ffff) | 0x00060000;
  }
}
