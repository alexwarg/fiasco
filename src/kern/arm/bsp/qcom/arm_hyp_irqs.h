#pragma once

#include <globalconfig.h>
#include_next <arm_hyp_irqs.h>

template<>
struct Hyp_irqs<void> : Hyp_irqs<int>
{
#ifdef CONFIG_HAVE_ARM_GICV2
  constexpr static int vgic   = 16 + 0;
  constexpr static int vtimer = 16 + 4;
#endif
#ifdef CONFIG_PF_QCOM_SM8150
  constexpr static int vtimer = 16 + 4;
#endif
};

