#pragma once

namespace Outer_cache
{
  inline void platform_init_post()
  {}

  static Mword platform_init(Mword aux_control)
  {
    return aux_control;
  }
}
