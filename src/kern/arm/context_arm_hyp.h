#pragma once

#include <context_arch_bits.h>
#include <vcpu.h>
#include <hyp_vm_state.h>

template<typename CTXT>
class Context_arch_base :  public Context_arch_bits
{
private:
  template<typename T>
  friend class Thread_vcpu_arch_t;

  using Context = CTXT;

  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

  bool is_ext_vcpu() const
  {
    return _this()->state.has(Thread_ext_vcpu_enabled);
  }

public:
  using Vm_state = Hyp_vm_state;

  void arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
  {
    if (!is_ext_vcpu())
      {
        _cpu_state.tpidruro(vcpu->host.tpidruro);
        // vCPU user state has TGE set, so we need to reload HCR here
        _hyp.hcr = Cpu::Hcr_non_vm_bits;
        if (do_load)
          {
            Cpu::hcr(_hyp.hcr);
            _cpu_state.load_tpidruro();
          }
        return;
      }

    Vm_state *v = vm_state(vcpu);

    v->guest_regs.hcr = _hyp.hcr;
    bool const all_priv_vm = !(_hyp.hcr & Cpu::Hcr_tge);
    if (all_priv_vm)
      {
        // save guest state, load full host state
        if (do_load)
          {
            v->switch_to_host(vcpu->_regs.tpidruro);
            Gic_h_global::gic->save_and_disable(&v->gic);
          }
        else
          arm_ext_vcpu_switch_to_host_no_load(vcpu, v);
      }

    _cpu_state.tpidruro(vcpu->host.tpidruro);
    _hyp.hcr = Cpu::Hcr_host_bits;
    if (do_load)
      v->load_host_regs(vcpu->host.tpidruro);
  }

  void switch_vm_state(Context *t)
  {
    _hyp.save();
    _cpu_state.store_tpidruro();
    t->_hyp.load();

    Mword _state = _this()->state.dirty();
    Mword _to_state = t->state.dirty();
    if (!((_state | _to_state) & Thread_ext_vcpu_enabled))
      return;

    // either current or next has extended vCPU enabled

    bool vgic = false;

    if (_state & Thread_ext_vcpu_enabled)
      {
        Vm_state *v = vm_state(_this()->vcpu_state().access());
        v->save();

        if ((_state & Thread_vcpu_user))
          vgic = Gic_h_global::gic->save_full(&v->gic);
      }

    if (_to_state & Thread_ext_vcpu_enabled)
      {
        Vm_state const *v = vm_state(t->vcpu_state().access());
        v->load(t->_hyp.hcr & (Cpu::Hcr_tge | Cpu::Hcr_dc));

        if (_to_state & Thread_vcpu_user)
          {
            Vm_state::load_cnthctl(Vm_state::Guest_cnthctl);
            Gic_h_global::gic->load_full(&v->gic, vgic);
          }
        else
          {
            Vm_state::load_cnthctl(Vm_state::Host_cnthctl);
            if (vgic)
              Gic_h_global::gic->disable();
          }
      }
    else
      arm_hyp_load_non_vm_state(vgic);
  }

  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const
  {
    dst->copy(src);
    _hyp.sanitize_psr(&dst->psr);
  }

protected:
  explicit Context_arch_base(Mword *kernel_sp) noexcept
  : Context_arch_bits(kernel_sp)
  {}

  static Vm_state *vm_state(Vcpu_state *vs)
  { return reinterpret_cast<Vm_state *>(reinterpret_cast<char *>(vs) + 0x400); }

  void arch_vcpu_ext_shutdown()
  {
    if (!_this()->state.has(Thread_ext_vcpu_enabled))
      return;

    _this()->state.del_dirty(Thread_ext_vcpu_enabled);
    _hyp.hcr = Cpu::Hcr_non_vm_bits;
    arm_hyp_load_non_vm_state(true);
  }

  void arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load)
  {

    if (!is_ext_vcpu())
      {
        _hyp.hcr = Cpu::Hcr_non_vm_bits | Cpu::Hcr_tge;
        _cpu_state.tpidruro(vcpu->_regs.tpidruro);
        if (do_load)
          {
            Cpu::hcr(_hyp.hcr);
            _cpu_state.load_tpidruro();
          }
        return;
      }

    Vm_state *v = vm_state(vcpu);
    _hyp.hcr = access_once(&v->guest_regs.hcr) | Cpu::Hcr_must_set_bits;
    bool const all_priv_vm = !(_hyp.hcr & Cpu::Hcr_tge);

    if (all_priv_vm)
      {
        if (do_load)
          {
            v->switch_to_guest(&_hyp);
            Gic_h_global::gic->load_full(&v->gic, true);
          }
        else
          v->switch_to_guest_no_load(&_hyp);
      }

    if (do_load)
      vcpu->host.tpidruro = Vm_state::load_guest_regs(_hyp.hcr, vcpu->_regs.tpidruro);
    else
      vcpu->host.tpidruro = _cpu_state.tpidruro();

    _cpu_state.tpidruro(vcpu->_regs.tpidruro);
  }

  void sanitize_user_state(Return_frame *dst) const
  {
    _hyp.sanitize_psr(&dst->psr);
  }

private:
  void arm_ext_vcpu_switch_to_host_no_load(Vcpu_state *vcpu, Vm_state *v)
  {
    vcpu->_regs.tpidruro   = _cpu_state.tpidruro();
    v->switch_to_host_no_load(&_hyp);
  }

  void arm_hyp_load_non_vm_state(bool vgic)
  {
    Vm_state::load_non_vm_state();
    if (vgic)
      Gic_h_global::gic->disable();
  }

protected:
  Context_hyp _hyp;
};

