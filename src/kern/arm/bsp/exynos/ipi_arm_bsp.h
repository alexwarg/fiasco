#pragma once

#include <globalconfig.h>
#include <ipi_arm_gic.h>

template<typename T>
struct Ipi_arch : Ipi_arm_gic<T>
{
  using Message = typename Ipi_arm_gic<T>::Message;
  static void softint_phys(Message m, Unsigned64 target)
  {
    Gic::primary->softint_phys(m, target);
  }
};


