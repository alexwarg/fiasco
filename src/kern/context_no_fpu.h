#pragma once

#include <context_base.h>

template<typename CTXT>
class Context_fpu_x
{
public:
  void spill_fpu_if_owner()
  {}

  static void spill_current_fpu(Cpu_number)
  {}

  void spill_fpu()
  {}

  void release_fpu_if_owner()
  {}

protected:
  void switch_fpu(Context *)
  {}

  void vcpu_enable_fpu_if_disabled(Mword thread_state)
  { (void) thread_state; }

};
