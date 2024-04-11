#pragma once

#include <context_arch_bits.h>
#include <context_vcpu_arch_base.h>
#include <vcpu.h>
#include <entry_frame.h>

template<typename CTXT>
class Context_arch_base :
  public Context_arch_bits,
  public Context_vcpu_arch_base
{
protected:
  explicit Context_arch_base(Mword *kernel_sp) noexcept
  : Context_arch_bits(kernel_sp)
  {}

  void sanitize_user_state(Return_frame *dst) const
  {
    dst->psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
    dst->psr |= Proc::Status_mode_user | Proc::Status_always_mask;
  }

public:
  void arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
  {
    _cpu_state.tpidruro(vcpu->host.tpidruro);
    if (do_load)
      _cpu_state.load_tpidruro();
  }

  void arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load)
  {
    _cpu_state.tpidruro(vcpu->_regs.tpidruro);
    if (do_load)
      _cpu_state.load_tpidruro();
  }

  void switch_vm_state(Context *) {}

  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const
  { dst->copy_and_sanitize(src); }
};

