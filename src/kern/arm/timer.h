#pragma once

#include <globalconfig.h>

#ifndef CONFIG_ARM_GENERIC_TIMER
#error ARM BSP needs to provide a timer.h if not using ARM generic timers
#endif

#include <timer_arm_generic.h>

using Timer = Timer_generic_timer;
