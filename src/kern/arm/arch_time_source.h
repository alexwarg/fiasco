#pragma once

#include <globalconfig.h>
#ifdef CONFIG_ARM_GENERIC_TIMER
#include <arch_time_source-generic_timer.h>
using Arch_time_source = Arch_time_source_generic_timer;
#else
// dummy for JDB
struct Arch_time_source
{
  static Unsigned64 ts_to_ns(Unsigned64 ts)
  { return ts; }
};
#endif

