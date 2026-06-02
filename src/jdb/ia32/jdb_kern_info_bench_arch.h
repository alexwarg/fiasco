
#pragma once

#include <globalconfig.h>
#include <timer_tick.h>
#include <div32.h>

namespace Jdb_kern_info_arch {

inline Unsigned64 get_time_now()
{ return Cpu::rdtsc(); }

[[gnu::unused]]
static void show_time(Unsigned64 time, Unsigned32 rounds,
                      const char *descr)
{
  Unsigned64 cycs = div32(time, rounds);
  printf("  %-24s %6llu.%llu cycles\n",
      descr, cycs, div32(time-cycs*rounds, rounds/10));
}

#ifdef CONFIG_MP
static inline void stop_timer()
{
  Timer_tick::set_vectors_stop();
}
#endif

}

