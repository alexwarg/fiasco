#pragma once

#include <thread_vcpu_arm.h>
#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

#include <hyp_vm_state.h>

template<typename BASE>
class Thread_vcpu_arch_t : public Thread_vcpu_arm_t<BASE>
{
public:
  static bool ext_vcpu_available()
  { return true; }

  static void init_state(Context *c, Vcpu_state *vcpu_state, bool ext)
  {
    if (!ext || (c->state.has(Thread_ext_vcpu_enabled)))
      return;

    using Vm_state = Hyp_vm_state;

    Vm_state::Vm_info *info
      = offset_cast<Vm_state::Vm_info *>(vcpu_state, Config::Ext_vcpu_state_offset);

    info->setup();

    Vm_state *v = c->vm_state(vcpu_state);

    v->sctlr = Cpu::Sctlr_el1_generic;
    v->actlr = 0;
    v->amair = 0;

    v->cntvoff = 0;
    v->guest_regs.hcr = Cpu::Hcr_tge;
    v->guest_regs.sctlr = 0;

    v->host_regs.hcr = Cpu::Hcr_host_bits;

    Gic_h_global::gic->setup_state(&v->gic);

    v->vmpidr = c->_hyp.vmpidr;
    v->vpidr = c->_hyp.vpidr;

    if (current() == c)
      {
        asm volatile ("msr SCTLR_EL1, %x0"   : : "r"(v->sctlr));
        asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(v->cntvoff));
        asm volatile ("msr HSTR_EL2, %x0" : : "r"(Cpu::Hstr_vm)); // HSTR
      }
  }
};
#endif
