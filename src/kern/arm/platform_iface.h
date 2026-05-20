#pragma once

#include <types.h>

struct Platform_if
{
  virtual Address scu_phys() { return 0; }
  virtual void init() {}
  virtual void init_irqs() = 0;
  virtual void init_irqs_ap(Cpu_number cpu, bool resume) = 0;

  static Platform_if *pf;
};

struct Platform_if_base : Platform_if
{
  Platform_if_base() { pf = this; }
};
