#pragma once

#include <globalconfig.h>

#if defined (CONFIG_ARM_MPTIMER)

#include <timer_realview_mptimer.h>
using Timer = Timer_realview_mptimer;

#elif defined (CONFIG_ARM_GENERIC_TIMER)

// use a generic timer with defaults
#include_next <timer.h>

#elif defined (CONFIG_ARM_SP804_TIMER)

#include <timer_realview_sp804.h>
using Timer = Timer_realview_sp804;

#endif
