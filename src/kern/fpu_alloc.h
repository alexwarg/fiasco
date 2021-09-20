#pragma once

#include <fpu_state_ptr.h>
#include <fpu.h>
#include <globalconfig.h>

class Ram_quota;

namespace Fpu_alloc
{
  void free_state(Fpu_state_ptr &s);
#ifndef CONFIG_FPU_ALLOC_TYPED
  bool alloc_state(Ram_quota *q, Fpu_state_ptr &s);
  inline void ensure_compatible_state(Ram_quota *, Fpu_state_ptr &, Fpu_state_ptr const &)
  {}
#else
  bool alloc_state(Ram_quota *q, Fpu_state_ptr &s,
                   Fpu::State_type type = Fpu::Default_state_type);
  inline void ensure_compatible_state(Ram_quota *q, Fpu_state_ptr &to,
                                      Fpu_state_ptr const &from)
  {
    if (to.get()->type() == from.get()->type())
      return;

    free_state(to);
    alloc_state(q, to, from.get()->type());
  }
#endif
}

