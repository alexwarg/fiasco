#pragma once

namespace Outer_cache
{
  inline void platform_init_post()
  {}

  static Mword platform_init(Mword aux_control)
  {
    // 64k way, 8-way associativityciativity
    return (aux_control & 0xf2f0ffff) | 0x00060000;
  }
}
