#pragma once

#include <arm_scu_base.h>
#include <globalconfig.h>

struct Scu : Scu_base
{
#if defined (CONFIG_PF_EXYNOS4) && !defined(CONFIG_ARM_EM_NS)
  enum
  {
    Available   = 1,
    Bsp_enable_bits = Control_scu_standby,
  };
#endif
#if defined (CONFIG_PF_EXYNOS5) && defined(CONFIG_ARM_EM_NS)
  enum
  {
    Available = 0,
    Bsp_enable_bits = 0,
  };
#endif

  template<typename ...T>
  Scu(T &&...args) : Scu_base(cxx::forward<T>(args)...) {}
};
