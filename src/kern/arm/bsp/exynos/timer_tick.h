#pragma once

#include <globalconfig.h>
#ifdef CONFIG_PF_EXYNOS_TIMER_GEN
#include <timer_tick-single-vector.h>
struct Timer_tick : Timer_tick_single_vector<Timer_tick> {};
#endif
#ifdef CONFIG_PF_EXYNOS_TIMER_MCT
#include <timer_tick-exynos-mct.h>
using Timer_tick = Timer_tick_exynos_mct;
#endif
#ifdef CONFIG_PF_EXYNOS_TIMER_MP
#include <timer_tick-single-vector.h>
struct Timer_tick : Timer_tick_single_vector<Timer_tick> {};
#endif
#ifdef CONFIG_PF_EXYNOS_TIMER_PWM
#ifdef CONFIG_MP
#include <timer_tick-broadcast.h>
struct Timer_tick : Timer_tick_broadcast<Timer_tick> {};
#else
#include <timer_tick-single-vector.h>
struct Timer_tick : Timer_tick_single_vector<Timer_tick> {};
#endif
#endif
