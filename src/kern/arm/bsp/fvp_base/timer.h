#pragma once

#include <timer_arm_generic.h>

struct Timer : Timer_generic_timer
{
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 29;
      case Generic_timer::Virtual:  return 27;
      case Generic_timer::Hyp:      return 26;
      };
  }

  static void init(Cpu_number cpu)
  {
    if (!check_and_disable(cpu))
      return;

    if (is_boot_cpu(cpu) && _freq0 == 0)
      set_freq0(100000000);

    finalize_init(cpu);
  }
};
