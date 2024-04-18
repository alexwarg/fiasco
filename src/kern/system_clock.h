#pragma once

#include <globalconfig.h>

#ifdef CONFIG_SYNC_CLOCK
#include <system_clock_hw_sync.h>
#include <arch_time_source.h>
using System_clock = System_clock_hw_sync<Arch_time_source>;
#else
#include <system_clock_simple.h>
using System_clock = System_clock_simple;
#endif
