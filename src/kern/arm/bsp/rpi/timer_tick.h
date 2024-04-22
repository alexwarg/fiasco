#pragma once

#include <globalconfig.h>
#if defined (CONFIG_PF_RPI_RPI2) || defined (CONFIG_RPI_RPI3)
#include <timer_tick-arm-rpi.h>
using Timer_tick = Timer_tick_arm_rpi;
#else
#include_next <timer_tick.h>
#endif
