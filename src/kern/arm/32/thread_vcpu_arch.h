#pragma once

#include <thread_vcpu_arm.h>
#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

template<typename BASE>
class Thread_vcpu_arch_t : public Thread_vcpu_arm_t<BASE>
{
public:
  static void init_state(Context *c, Vcpu_state *vcpu_state, bool ext)
  {
    if (!ext || (c->state.has(Thread_ext_vcpu_enabled)))
      return;

    using Vm_state = Hyp_vm_state;

    Vm_state::Vm_info *info
      = offset_cast<Vm_state::Vm_info *>(vcpu_state, Config::Ext_vcpu_infos_offset);

    info->setup();

    Vm_state *v = c->vm_state(vcpu_state);

    v->csselr = 0;
    v->sctlr = Vm_state::arm_host_sctlr();
    v->actlr = 0;
    v->cpacr = 0x5f55555;
    v->fcseidr = 0;
    v->vbar = 0;
    v->amair0 = 0;
    v->amair1 = 0;
    v->cntvoff = 0;

    v->guest_regs.hcr = Cpu::Hcr_tge | Cpu::Hcr_must_set_bits;
    v->guest_regs.sctlr = 0;

    v->host_regs.hcr = Cpu::Hcr_host_bits;

    Gic_h_global::gic->setup_state(&v->gic);

    // use the real MPIDR as initial value, we might change this later
    // on and mask bits that should not be known to the user
    asm ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v->vmpidr));

    // use the real MIDR as initial value
    asm ("mrc p15, 0, %0, c0, c0, 0" : "=r" (v->vpidr));

    if (current() == c)
      v->load(false);
  }
};
#endif
