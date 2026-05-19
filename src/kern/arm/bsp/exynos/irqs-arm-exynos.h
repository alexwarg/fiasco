#pragma once

#include <types.h>

struct Irqs_arm_exynos
{
  static void reinit(Cpu_number cpu);
  static void set_pending_irq(unsigned group32num, Unsigned32 val);
};
