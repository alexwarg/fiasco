#pragma once

#include <mem_layout_arm_bsp.h>
#include <kmem.h>

namespace Outer_cache
{
  inline void platform_init_post()
  {}

  static Mword platform_init()
  {
    using namespace Priv;
    l2cxx0.construct(Kmem::mmio_remap(Mem_layout_arm_bsp::L2cxx0_phys_base, 0x1000));
    return l2cxx0->read<Unsigned32>(L2cxx0::AUX_CONTROL);
  }
}
