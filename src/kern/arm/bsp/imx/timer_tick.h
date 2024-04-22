#pragma once

#include <globalconfig.h>
#if defined (CONFIG_PF_IMX_6_TIMER_EPIT) && defined (CONFIG_MP)
#include <timer_tick-broadcast.h>
struct Timer_tick : Timer_tick_broadcast<Timer_tick> {};
#else
#include_next <timer_tick.h>
#endif
