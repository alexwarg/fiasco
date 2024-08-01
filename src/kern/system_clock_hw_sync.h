#pragma once

#include <types.h>
#include <kip.h>
#include <globals.h>
#include <config.h>

#include <warn.h>

template<typename TIME_SRC>
class System_clock_hw_sync
{
public:
  static void init()
  {
    TIME_SRC::init_system_clock();
    Cpu_time time = TIME_SRC::time_us();
    if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on boot CPU\n", time);
  }

  static void check_ap_cpu(Cpu_number)
  {
  }

  static void update(Cpu_number) {}

  static Unsigned64 clock()
  {
    return TIME_SRC::time_us();
  }

  // this function must not rely on any kernel stack, or
  // current(), current_cpu() etc. functions. It is also
  // not allowed to have side effects.
  static Unsigned64 aux_clock()
  {
    return TIME_SRC::time_us();
  }
};
