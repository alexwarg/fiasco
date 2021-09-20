#pragma once

#include <fpu.h>
#include <fpu_alloc.h>
#include <fpu_state_ptr.h>
#include <context.h>

#include <cassert>

template<typename TH>
class Thread_lazy_fpu_x
{
private:
  using Thread = TH;

  Thread *_this()
  { return static_cast<Thread *>(this); }

  Thread const *_this() const
  { return static_cast<Thread const *>(this); }

public:

  // do nothing and return success for lazy FPU
  bool alloc_eager_fpu_state()
  { return true; }

  int switchin_fpu(bool alloc_new_fpu = true)
  {
    if (_this()->state.has(Thread_vcpu_fpu_disabled))
      return 0;

    (void)alloc_new_fpu;

    Fpu &f = Fpu::fpu.current();
    // If we own the FPU, we should never be getting an "FPU unavailable" trap
    assert (f.owner() != _this());

    Fpu_state_ptr &_fpu_state = _this()->fpu_state();

    // Allocate FPU state slab if we didn't already have one
    if (!_fpu_state
        && (EXPECT_FALSE(!alloc_new_fpu
                         || !Fpu_alloc::alloc_state(_this()->quota(), _fpu_state))))
      return 0;

    // Enable the FPU before accessing it, otherwise recursive trap
    f.enable();

    // Save the FPU state of the previous FPU owner (lazy) if applicable
    if (f.owner())
      f.owner()->spill_fpu();

    // Become FPU owner and restore own FPU state
    f.restore_state(_fpu_state.get());

    _this()->state.add_dirty(Thread_fpu_owner);
    f.set_owner(_this());
    return 1;
  }

  void transfer_fpu(Thread *to) //, Trap_state *trap_state, Utcb *to_utcb)
  {
    if (to->fpu_state())
      Fpu_alloc::free_state(to->fpu_state());

    to->fpu_state() = cxx::move(_this()->fpu_state());

    if (_this()->home_cpu() != to->home_cpu())
      {
        assert (!to->state.has(Thread_fpu_owner));
        assert (!_this()->state.has(Thread_fpu_owner));
        return;
      }

    assert (current() == _this() || current() == to);

    Fpu &f = Fpu::fpu.current();

    f.disable(); // it will be reenabled in switch_fpu

    if (EXPECT_FALSE(f.owner() == to))
      {
        assert (to->state.has(Thread_fpu_owner));

        f.set_owner(0);
        to->state.del_dirty(Thread_fpu_owner);
      }
    else if (f.owner() == _this())
      {
        assert (_this()->state.has(Thread_fpu_owner));

        _this()->state.del_dirty(Thread_fpu_owner);

        to->state.add_dirty (Thread_fpu_owner);
        f.set_owner(to);
        if (EXPECT_FALSE(current() == to))
          f.enable();
      }
  }

protected:
  void free_fpu_state()
  {
    Fpu_alloc::free_state(_this()->fpu_state());
  }
};

template<typename TH>
using Thread_fpu_x = Thread_lazy_fpu_x<TH>;
