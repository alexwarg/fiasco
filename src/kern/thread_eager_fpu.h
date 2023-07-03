#pragma once

#include <fpu.h>
#include <fpu_alloc.h>
#include <fpu_state.h>

#include <cassert>
#include <cstring>
#include <panic.h>

template<typename TH>
class Thread_eager_fpu_x
{
private:
  using Thread = TH;

  Thread *_this()
  { return static_cast<Thread *>(this); }

  Thread const *_this() const
  { return static_cast<Thread const *>(this); }

public:
  int switchin_fpu(bool alloc_new_fpu = true)
  {
    if (_this()->state.has(Thread_vcpu_fpu_disabled))
      return 0;

    (void)alloc_new_fpu;
    panic("must not see any FPU trap with eager FPU\n");
  }

  bool alloc_eager_fpu_state()
  {
    return Fpu_alloc::alloc_state(_this()->quota(), _this()->fpu_state());
  }

  void transfer_fpu(Thread *to) //, Trap_state *trap_state, Utcb *to_utcb)
  {
    auto *curr = current();
    if (_this() == curr)
      Fpu::save_state(to->fpu_state());
    else if (curr == to)
      Fpu::restore_state(_this()->fpu_state());
    else
      memcpy(to->fpu_state()->state_buffer(), _this()->fpu_state()->state_buffer(), Fpu::state_size());
  }

protected:
  void free_fpu_state()
  {
    Fpu_alloc::free_state(_this()->fpu_state());
  }
};

template<typename TH>
using Thread_fpu_x = Thread_eager_fpu_x<TH>;

