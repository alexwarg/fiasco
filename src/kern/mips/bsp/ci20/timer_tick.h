#pragma once

// default to cp0 timer
#include <timer_tick-single-vector.h>
struct Timer_tick : Timer_tick_single_vector<Timer_tick, true> {};

