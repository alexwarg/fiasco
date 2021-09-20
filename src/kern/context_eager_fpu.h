#pragma once

#include <fpu_state_ptr.h>
#include <fpu.h>
#include <context_base.h>

#include <cassert>

template<typename CTXT>
class Context_fpu_x
{
private:
  using Context = CTXT;

  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const*>(this); }

  Fpu_state_ptr &fpu_state() { return _this()->fpu_state(); }

public:
  void spill_fpu()
  {
    assert (fpu_state());

    // Save the FPU state of the previous FPU owner
    Fpu::save_state(fpu_state().get());
  }

  void spill_fpu_if_owner()
  {
    if (current() != _this())
      return;

    spill_fpu();
  }

  static void spill_current_fpu(Cpu_number cpu)
  {
    (void)cpu;
    assert (cpu == current_cpu());

    static_cast<Context *>(current())->spill_fpu();
  }

  void release_fpu_if_owner()
  {}

protected:
  void switch_fpu(Context *t)
  {
    Fpu &f = Fpu::fpu.current();

    if (_this()->state.has(Thread_vcpu_fpu_disabled))
      f.enable();

    spill_fpu();
    f.restore_state(t->fpu_state().get());

    if (t->state.has(Thread_vcpu_fpu_disabled))
      f.disable();
  }

  void vcpu_enable_fpu_if_disabled(Mword thread_state)
  {
    if (thread_state & Thread_vcpu_fpu_disabled)
      Fpu::fpu.current().enable();
  }
};

