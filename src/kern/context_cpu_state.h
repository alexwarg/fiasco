#pragma once

#include <types.h>
#include <context_space_ref.h>
#include <context_cpu_state_arch.h>

class Context_cpu_state_base
{
public:
  explicit Context_cpu_state_base(Mword *kernel_sp)
  : kernel_sp(kernel_sp)
  {}

public:
  Context_space_ref space;
  Mword *kernel_sp;
};

using Context_cpu_state = Context_cpu_state_arch<Context_cpu_state_base>;
