#pragma once

#include <timer_arm_generic.h>

struct Timer : Timer_generic_timer
{

#ifdef CONFIG_HAVE_ARM_GICV2
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 16 + 3; // (non-secure)
      case Generic_timer::Virtual:  return 16 + 4;
      case Generic_timer::Hyp:      return 16 + 1;
      };
  }
#endif

#ifdef CONFIG_PF_QCOM_SM8150
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 16 + 2; // (non-secure)
      case Generic_timer::Virtual:  return 16 + 3;
      case Generic_timer::Hyp:      return 16 + 0;
      };
  }
#endif
};
