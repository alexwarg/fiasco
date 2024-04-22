#pragma once

// this is a generic version absed if the BSP does not provide
// a dedicated timer_tick.h

#include <timer_tick-single-vector.h>
struct Timer_tick : Timer_tick_single_vector<Timer_tick> {};

