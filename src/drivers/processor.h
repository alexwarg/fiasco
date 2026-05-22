#pragma once

#include "types.h"
#include "std_macros.h"
#include <processor-arch.h>

class Proc : public Proc_arch<Proc>
{
public:
  enum
  {
    Is_32bit = sizeof(Mword) == 4,
    Is_64bit = sizeof(Mword) == 8,
  };

  static inline void preemption_point()
  {
    Proc_arch<Proc>::sti();
    Proc_arch<Proc>::irq_chance();
    Proc_arch<Proc>::cli();
  }
};
