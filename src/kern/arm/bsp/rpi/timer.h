#pragma once

#include <globalconfig.h>

#if defined (CONFIG_PF_RPI_RPI1) || defined (CONFIG_PF_RPI_RPIZW)

#include <timer_arm_rpi.h>
using Timer = Timer_arm_rpi;

#elif defined (CONFIG_PF_RPI_RPI2) || defined  (CONFIG_PF_RPI_RPI3)

#include <timer_arm_generic.h>

struct Timer : Timer_generic_timer
{
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 1; // use the non-secure IRQ
      case Generic_timer::Virtual:  return 3;
      case Generic_timer::Hyp:      return 2;
      };
  }
};

#else

// for RPI4+ use the default for the generic timer

#include_next <timer.h>

#endif
