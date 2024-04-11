#pragma once

#include <types.h>

template<typename BASE>
class Context_cpu_state_arch : public BASE
{
public:
  Mword ulr;

  explicit Context_cpu_state_arch(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}
};
