#pragma once

#include <globalconfig.h>
#ifdef CONFIG_SCHED_APIC
#include <timer_tick-apic.h>
using Timer_tick = Timer_tick_apic;
#else
#include <timer_tick-ia32.h>
using Timer_tick = Timer_tick_ia32;
#endif
