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
    Mword aux_control = l2cxx0->read<Unsigned32>(L2cxx0::AUX_CONTROL);

    l2cxx0->write<Mword>(0x331, L2cxx0::TAG_RAM_CONTROL);
    l2cxx0->write<Mword>(0x441, L2cxx0::DATA_RAM_CONTROL);

    aux_control &= 0x8200c3fe;
    aux_control |=   (1 <<  0)  // Full Line of Zero Enable
                   | (4 << 17)  // 128kb waysize
                   | (1 << 28)  // data prefetch
                   | (1 << 29)  // insn prefetch
                   | (1 << 30)  // early BRESP enable
                   ;
    return aux_control;
  }
}
