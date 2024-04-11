#pragma once

#include <x86desc.h>
#include <types.h>
#include <context_arch_bits.h>
#include <context_vcpu_arch_base.h>
#include <context_cpu_state.h>

#include <cassert>

template<typename CTXT>
class Context_arch_x
: public Context_arch_bits,
  public Context_vcpu_arch_base
{
private:
  using Context = CTXT;
  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

protected:
  Context_arch_x() noexcept
  : Context_arch_bits(reinterpret_cast<Mword *>(_this()->regs()))
  {}

  /**
   * Thread context switchin.  Called on every re-activation of a thread
   * (switch_exec()).  This method is public only because it is called from
   * from assembly code in switch_cpu().
   */
  void switchin_context_arch(Context *from)
  {
    from->handle_lock_holder_preemption();
    // Set kernel-esp in case we want to return to the user.
    // kmem::kernel_sp() returns a pointer to the kernel SP (in the
    // TSS) the CPU uses when next switching from user to kernel mode.
    // regs() + 1 returns a pointer to the end of our kernel stack.
    Cpu::cpus.current().kernel_sp() = reinterpret_cast<Address>(_this()->regs() + 1);

    // switch to our page directory if necessary
    _this()->vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

    // load new segment selectors
    load_segments();
  }


public:
  void prepare_switch_to(void (*fptr)())
  {
    *reinterpret_cast<void(**)()> (--_cpu_state.kernel_sp) = fptr;
  }

  void arch_setup_utcb_ptr()
  {
    auto &u = _this()->_utcb;
    u.access()->utcb_addr = (Mword)u.usr().get();
    arch_bits_setup_utcb_ptr(&u.usr()->utcb_addr);
  }

  void arch_update_vcpu_state(Vcpu_state *)
  {}

  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const
  { dst->copy_and_sanitize(src); }
};
