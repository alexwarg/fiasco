#pragma once

#include <types.h>

struct Mips_bsp_irqs
{
  static void init(Cpu_number cpu);
  static void init_ap(Cpu_number cpu);
};

