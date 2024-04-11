#pragma once

#include <fpu_state.h>
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

  Fpu_state *fpu_state() { return _this()->fpu_state(); }

public:
  void spill_fpu()
  {
    // If we own the FPU, we should never be getting an "FPU unavailable" trap
    assert (Fpu::fpu.current().owner() == _this());
    assert (_this()->state.has(Thread_fpu_owner));
    assert (fpu_state());

    // Save the FPU state of the previous FPU owner (lazy) if applicable
    Fpu::save_state(fpu_state());
    _this()->state.del_dirty(Thread_fpu_owner);
  }

  void spill_fpu_if_owner()
  {
    // spill FPU state into memory before migration
    if (!_this()->state.has(Thread_fpu_owner))
      return;

    Fpu &f = Fpu::fpu.current();

    if (current() != _this())
      f.enable();

    spill_fpu();
    f.set_owner(nullptr);
    f.disable();
  }

  static void spill_current_fpu(Cpu_number cpu)
  {
    (void)cpu;
    assert (cpu == current_cpu());

    Fpu &f = Fpu::fpu.current();
    if (f.owner())
      {
        f.enable();
        static_cast<Context *>(f.owner())->spill_fpu();
        f.set_owner(nullptr);
        f.disable();
      }
  }

  void release_fpu_if_owner()
  {
    // If this context owns the FPU, no one owns it now
    Fpu &f = Fpu::fpu.current();
    if (f.is_owner(_this()))
      {
        f.set_owner(nullptr);
        f.disable();
      }
  }

protected:
  /**
   * When switching away from the FPU owner, disable the FPU to cause
   * the next FPU access to trap.
   * When switching back to the FPU owner, enable the FPU so we don't
   * get an FPU trap on FPU access.
   */
  void switch_fpu(Context *t)
  {
    Fpu &f = Fpu::fpu.current();
    if (f.is_owner(_this()))
      f.disable();
    else if (f.is_owner(t) && !t->state.has(Thread_vcpu_fpu_disabled))
      f.enable();
  }

  void vcpu_enable_fpu_if_disabled(Mword thread_state)
  {
    if ((thread_state & (Thread_fpu_owner | Thread_vcpu_fpu_disabled))
        == (Thread_fpu_owner | Thread_vcpu_fpu_disabled))
      Fpu::fpu.current().enable();
  }
};


