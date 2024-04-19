#pragma once

#include <cpu.h>

namespace Kip_init
{
  void init();
  void init_kip_clock();
  void init_freq(Cpu const &cpu);
}
