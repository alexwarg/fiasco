#pragma once

#include <types.h>
#include <config.h>

class Timer_apic
{
public:
  static constexpr Unsigned64 Infinite_timeout = UINT64_MAX;

  static int irq()
  { return -1; }

  static void acknowledge()
  {}

  static void init(Cpu_number);

  static void update_timer(Unsigned64 wakeup)
  {
    if constexpr (Config::Scheduler_one_shot)
      update_one_shot(wakeup);
  }

private:
  static void update_one_shot(Unsigned64 wakeup);

};
