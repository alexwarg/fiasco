#pragma once

#include <types.h>
#include <config.h>
#include <globals.h>
#include <kip.h>
#include <cpu.h>

#include <globalconfig.h>

#ifdef CONFIG_SCHED_APIC
#include <timer_apic.h>
using Timer_impl = Timer_apic;
#endif
#ifdef CONFIG_SCHED_RTC
#include <timer_rtc.h>
using Timer_impl = Timer_rtc;
#endif
#ifdef CONFIG_SCHED_PIT
#include <timer_pit.h>
using Timer_impl = Timer_pit;
#endif
#ifdef CONFIG_SCHED_HPET
#include <timer_hpet.h>
using Timer_impl = Timer_hpet;
#endif

class Timer : public Timer_impl
{
public:
  static void enable()
  {}
};
