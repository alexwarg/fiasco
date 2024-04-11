#pragma once

#include <context_base.h>
#include <ku_mem_ptr.h>
#include <vcpu.h>

class Context_vcpu_base
{
protected:
  static char upcall[] asm ("leave_by_vcpu_upcall");

public:
  // override these functions per arch if needed
  void vcpu_pv_switch_to_kernel(Vcpu_state *, bool) {}
  void vcpu_pv_switch_to_user(Vcpu_state *, bool) {}
};

template<typename CTXT>
class Context_vcpu_x : public Context_vcpu_base
{
private:
  using Context = CTXT;

  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

  bool is_vcpu() const
  { return EXPECT_FALSE(_this()->state.has(Thread_vcpu_enabled)); }

  static bool is_vcpu(Mword state)
  { return EXPECT_FALSE(state & Thread_vcpu_enabled); }

protected:
  Ku_mem_ptr<Vcpu_state> _vcpu_state;

public:
  Ku_mem_ptr<Vcpu_state> const &vcpu_state() const
  { return _vcpu_state; }


  bool vcpu_irqs_enabled(Vcpu_state *vcpu) const
  {
    return is_vcpu() && vcpu->irqs_enabled();
  }

  bool vcpu_pagefaults_enabled(Vcpu_state *vcpu) const
  {
    return is_vcpu() && vcpu->pf_enabled();
  }

  bool vcpu_exceptions_enabled(Vcpu_state *vcpu) const
  {
    return is_vcpu() && vcpu->exc_enabled();
  }

  void vcpu_set_irq_pending()
  {
    if (is_vcpu())
      vcpu_state().access()->set_irq_pending();
  }

  Space *vcpu_user_space() const
  { return _this()->_space.vcpu_user(); }

  Mword vcpu_disable_irqs()
  {
    if (is_vcpu())
      return vcpu_state().access()->disable_irqs();
    return 0;
  }

  void vcpu_restore_irqs(Mword irqs)
  {
    if (is_vcpu())
      vcpu_state().access()->restore_irqs(irqs);
  }

  void vcpu_save_state_and_upcall()
  {
    _this()->_exc_cont.activate(_this()->regs(), upcall);
  }

  bool vcpu_enter_kernel_mode(Vcpu_state *vcpu)
  {
    unsigned s = _this()->state();
    if (! is_vcpu(s))
      return false;

    _this()->state.del_dirty(Thread_vcpu_user);
    vcpu->kern_entry(_this()->regs());

    if (!_this()->_space.user_mode())
      return false;

    _this()->_space.user_mode(false);
    _this()->state.del_dirty(Thread_vcpu_fpu_disabled);

    bool load_cpu_state = current() == _this();

    _this()->arch_load_vcpu_kern_state(vcpu, load_cpu_state);
    _this()->vcpu_pv_switch_to_kernel(vcpu, load_cpu_state);

    if (!load_cpu_state)
      return false;

    _this()->vcpu_enable_fpu_if_disabled(s);

    // Space::switchin_context() may optimize the switch of a thread
    // in vCPU user mode to vCPU kernel mode.
    _this()->space()->switchin_context(vcpu_user_space(),
        Mem_space::Vcpu_user_to_kern);

    return true;
  }


};
