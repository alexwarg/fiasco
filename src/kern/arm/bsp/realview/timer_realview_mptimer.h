#pragma once

#include <timer_arm_mptimer.h>

struct Timer_realview_mptimer : Timer_arm_mptimer
{
  static void init(Cpu_number cpu);
};
