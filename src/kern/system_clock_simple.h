#pragma once

#include <types.h>
#include <kip.h>
#include <globals.h>
#include <config.h>

class System_clock_simple
{
public:
  static void init()
  {
    Kip::k()->set_clock(0);
  }

  static void check_ap_cpu(Cpu_number)
  {}

  static Unsigned64 clock()
  {
    return Kip::k()->clock();
  }

  // this function must not rely on any kernel stack, or
  // current(), current_cpu() etc. functions. It is also
  // not allowed to have side effects.
  static Unsigned64 aux_clock()
  {
    return Kip::k()->clock();
  }

  static void update(Cpu_number cpu)
  {
    if (cpu != Cpu_number::boot_cpu())
      return;

    Kip::k()->add_to_clock(Config::Scheduler_granularity);
  }
};

