#pragma once

#include <types.h>

struct Cpu_idle_iface
{
  virtual void idle() = 0;
};

