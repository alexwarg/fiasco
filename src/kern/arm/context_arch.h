#pragma once

#pragma once

#include <types.h>
#include <context_arch_bits.h>
#include <context_cpu_state.h>
#include <utcb_support.h>
#include <vcpu.h>
#include <mem.h>
#include <globalconfig.h>

#include <cassert>

#ifdef CONFIG_CPU_VIRT
#include <context_arm_hyp.h>
#else // CONFIG_CPU_VIRT
#include <context_arm_nohyp.h>
#endif // CONFIG_CPU_VIRT

template<typename CTXT>
class Context_arch_x : public Context_arch_base<CTXT>
{
private:
  using Context = CTXT;

  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

protected:
  using Context_arch_base<CTXT>::_cpu_state;

  Context_arch_x() noexcept
  : Context_arch_base<CTXT>(reinterpret_cast<Mword *>(_this()->regs()))
  {}

public:
  void prepare_switch_to(void (*fptr)())
  {
    _cpu_state.prepare_switch_to(fptr);
  }

  void fill_user_state()
  {
    this->bits_fill_user_state(_this()->regs());
  }

  void spill_user_state()
  {
    assert (current() == _this());
    this->bits_spill_user_state(_this()->regs());
  }

  void switch_cpu(Context *to)
  {
    _this()->update_consumed_time();

    spill_user_state();
    _cpu_state.store_tpidrurw();
    _this()->switch_vm_state(to);
    to->fill_user_state();
    to->_cpu_state.load_tpidrurw();
    to->_cpu_state.load_tpidruro();
    this->arm_switch_gp_regs(to);
  }

  void vcpu_pv_switch_to_kernel(Vcpu_state *, bool)
  {}

  void vcpu_pv_switch_to_user(Vcpu_state *, bool)
  {}

  void arch_setup_utcb_ptr()
  {
    _cpu_state.tpidrurw(_cpu_state.tpidruro(
          reinterpret_cast<Mword>(_this()->utcb().usr().get())));
  }

  /** Thread context switchin.  Called on every re-activation of a
      thread (switch_exec()).  This method is public only because it is
      called by an ``extern "C"'' function that is called
      from assembly code (call_switchin_context).
   */
  void switchin_context_arch(Context *from)
  {
    from->handle_lock_holder_preemption();

    // switch to our page directory if necessary
    _this()->vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

    Utcb_support::current(_this()->utcb().usr());
  }

#ifdef CONFIG_ARM_V6PLUS
  void arch_update_vcpu_state(Vcpu_state *vcpu)
  {
    vcpu->host.tpidruro = _cpu_state.tpidruro();
  }
#else // CONFIG_ARM_V6PLUS
  void arch_update_vcpu_state(Vcpu_state *)
  {}
#endif // CONFIG_ARM_V6PLUS

  class Kernel_mem_op
  {
  public:
    void set_ignore(bool ignore)
    {
      _ignore = ignore;
      Mem::barrier();
    }

    bool is_ignore() const
    {
      return _ignore;
    }

    void set_hit()
    {
      _hit = true;
    }

    bool hit_and_clear()
    {
      bool h = _hit;
      if (EXPECT_FALSE(h))
        _hit = false;
      return EXPECT_FALSE(h);
    }

  private:
    bool _ignore:1;
    bool _hit:1;
  };

  Kernel_mem_op kernel_mem_op;
};

