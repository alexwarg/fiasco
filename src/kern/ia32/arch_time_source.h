#pragma once

#include <cpu.h>

struct Arch_time_source
{
  static Cpu_time time_us()
  { return Cpu::cpus.cpu(Cpu_number::boot_cpu()).time_us(); }

  static void init_system_clock();
};
