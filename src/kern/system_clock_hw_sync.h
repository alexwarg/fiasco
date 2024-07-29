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
    Kip::k()->set_clock(time);
    if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on boot CPU\n", time);
  }

  static void check_ap_cpu(Cpu_number cpu)
  {
    Cpu_time time = TIME_SRC::time_us();
    Cpu_time ktime = Kip::k()->clock();
    Cpu_time delta = time < ktime ? ktime - time : time - ktime;
    if (delta >= 100000)
      WARN("KIP clock delta = %lld > 100ms: on CPU %u\n",
            delta, cxx::int_value<Cpu_number>(cpu));
  }

  static Unsigned64 clock()
  {
    Cpu_time time = TIME_SRC::time_us();
    if (current_cpu() == Cpu_number::boot_cpu())
      Kip::k()->set_clock(time);

     return time;
  }

  static void update(Cpu_number cpu)
  {
    if (cpu != Cpu_number::boot_cpu())
      return;

    Kip::k()->set_clock(TIME_SRC::time_us());
  }
};
