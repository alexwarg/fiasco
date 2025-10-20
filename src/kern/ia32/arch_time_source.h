#pragma once

#include <cpu.h>

struct Arch_time_source
{
  static Cpu_time time_us()
  { return Cpu::cpus.cpu(Cpu_number::boot_cpu()).time_us(); }

  static Unsigned64 ts_to_ns(Unsigned64 ts)
  { return Cpu::boot_cpu()->tsc_to_ns(ts); }

  constexpr static bool Ts_to_ns_woks = true;

  static void init_system_clock();
};
