#pragma once

#include <globalconfig.h>

#ifdef CONFIG_ARM_MPTIMER
#include <timer_arm_mptimer.h>
#ifdef CONFIG_PF_TEGRA2
using Timer = Timer_arm_mptimer_t<249999>;
#endif
#ifdef CONFIG_PF_TEGRA3
using Timer = Timer_arm_mptimer_t<499999>;
#endif
#endif

#ifdef CONFIG_PF_TEGRA_TIMER_TMR
#include <timer_tegra_tmr.h>
using Timer = Timer_tegra_tmr;
#endif
