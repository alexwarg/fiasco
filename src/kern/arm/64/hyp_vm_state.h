#pragma once

#include "hyp_vm_state_generic.h"

struct Context_hyp : Context_hyp_generic
{
public:
  Unsigned64 sp_el1;
  Unsigned64 elr_el1;
  Unsigned64 vbar;
  Unsigned32 cpacr = Cpu::Cpacr_el1_generic_hyp;
  // we need to store all banked registers for PL1 modes
  // because a hyp kernel runs applications in system mode (PL1)
  Unsigned32 spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und;
  Unsigned32 csselr;

  // VM / USER RO but VMM writable
  Unsigned64 vmpidr = 1UL << 31;
  Unsigned32 vpidr = Cpu::midr();

  void save();
  void load();

  [[gnu::nonnull]]
  void sanitize_psr(Mword *psr) const
  {
    unsigned const max_el = 1;

    if (*psr & 0x10)
      return;

    // set illegal execution state bit in PSR, this will trigger
    // an exception upon ERET
    if (((*psr & 0xf) >> 2) > max_el)
      *psr = *psr | (1UL << 20);
  }
};


class Hyp_vm_state : public Hyp_vm_state_generic
{
public:
  struct Regs_g
  {
    Unsigned64 hcr;

    Unsigned32 sctlr;
    Unsigned32 cpacr;
    Unsigned32 cntv_ctl;
    Unsigned32 _res;
  };

  struct Regs_h
  {
    Unsigned64 hcr;
  };

  typedef Gic_h_global::Arm_vgic Gic;

  /* The following part is our user API */
  Regs_h host_regs;
  Regs_g guest_regs;

  Unsigned64 cntvoff;

  Unsigned64 vmpidr;
  Unsigned32 vpidr;

  Unsigned32 _rsvd[3];

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
  Unsigned64 actlr;

  Unsigned64 tcr;
  Unsigned64 ttbr0;
  Unsigned64 ttbr1;

  Unsigned32 sctlr;
  Unsigned32 esr;

  Unsigned64 mair;
  Unsigned64 amair;

  Unsigned64 sp_el1;
  Unsigned64 elr_el1;

  Unsigned64 far;

  Unsigned32 afsr[2];

  Unsigned32 dacr32;
  Unsigned32 fpexc32;
  Unsigned32 ifsr32;

  void save()
  {
    // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(actlr));
    asm volatile ("mrs %x0, TCR_EL1"   : "=r"(tcr));
    asm volatile ("mrs %x0, TTBR0_EL1" : "=r"(ttbr0));
    asm volatile ("mrs %x0, TTBR1_EL1" : "=r"(ttbr1));

    asm volatile ("mrs %x0, SCTLR_EL1" : "=r"(sctlr));
    asm volatile ("mrs %x0, ESR_EL1"   : "=r"(esr));

    asm volatile ("mrs %x0, MAIR_EL1"  : "=r"(mair));
    asm volatile ("mrs %x0, AMAIR_EL1" : "=r"(amair));

    asm volatile ("mrs %x0, FAR_EL1"   : "=r"(far));

    asm volatile ("mrs %x0, AFSR0_EL1" : "=r"(afsr[0]));
    asm volatile ("mrs %x0, AFSR1_EL1" : "=r"(afsr[1]));

    asm volatile ("mrs %x0, DACR32_EL2" : "=r"(dacr32));
    //  asm volatile ("mrs %x0, FPEXC32_EL2" : "=r"(fpexc32));
    asm volatile ("mrs %x0, IFSR32_EL2" : "=r"(ifsr32));
  }

  void load(bool el0_only) const
  {
    // always trapped: asm volatile ("msr ACTLR_EL1, %0" : : "r"(v->actlr));
    asm volatile ("msr HSTR_EL2, %x0" : : "r"(Cpu::Hstr_vm)); // HSTR

    asm volatile ("msr TCR_EL1, %x0"   : : "r"(tcr));
    asm volatile ("msr TTBR0_EL1, %x0" : : "r"(ttbr0));
    asm volatile ("msr TTBR1_EL1, %x0" : : "r"(ttbr1));

    Unsigned32 _sctlr = access_once(&sctlr);
    if (el0_only)
      _sctlr &= ~Cpu::Cp15_c1_mmu;

    asm volatile ("msr SCTLR_EL1, %x0" : : "r"(_sctlr));
    asm volatile ("msr ESR_EL1, %x0"   : : "r"(esr));

    asm volatile ("msr MAIR_EL1, %x0"  : : "r"(mair));
    asm volatile ("msr AMAIR_EL1, %x0" : : "r"(amair));

    asm volatile ("msr FAR_EL1, %x0"   : : "r"(far));

    asm volatile ("msr AFSR0_EL1, %x0" : : "r"(afsr[0]));
    asm volatile ("msr AFSR1_EL1, %x0" : : "r"(afsr[1]));

    asm volatile ("msr DACR32_EL2, %x0"  : : "r"(dacr32));
    //  asm volatile ("msr FPEXC32_EL2, %x0" : : "r"(fpexc32));
    asm volatile ("msr IFSR32_EL2, %x0"  : : "r"(ifsr32));

    asm volatile ("msr VMPIDR_EL2, %x0" : : "r" (vmpidr));
    asm volatile ("msr VPIDR_EL2, %x0"  : : "r" (vpidr));
  }

  void switch_to_host(Mword tpidruro)
  {
    asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(tpidruro));
    asm volatile ("mrs %x0, SCTLR_EL1"   : "=r"(guest_regs.sctlr));
    asm volatile ("mrs %x0, CPACR_EL1"   : "=r"(guest_regs.cpacr));
    asm volatile ("msr CPACR_EL1, %x0"   : : "r"(Cpu::Cpacr_el1_generic_hyp));

    asm volatile ("mrs %x0, CNTV_CTL_EL0" : "=r" (guest_regs.cntv_ctl));
    // disable VTIMER
    asm volatile ("msr CNTV_CTL_EL0, %x0" : : "r"(0UL));
    asm volatile ("msr CNTHCTL_EL2, %x0"  : : "r"(Host_cnthctl));
    // disable all debug exceptions for non-vms, if we want debug
    // exceptions into JDB we need either per-thread or a global
    // setting for this value. (probably including the contextidr)
    asm volatile ("msr MDSCR_EL1, %x0" : : "r"(0UL));
    asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Cpu::Sctlr_el1_generic));
  }

  [[gnu::nonnull]]
  void switch_to_host_no_load(Context_hyp *hyp)
  {
    guest_regs.sctlr    = sctlr;
    guest_regs.cpacr    = hyp->cpacr;
    guest_regs.cntv_ctl = hyp->cntv_ctl;

    sctlr   = Cpu::Sctlr_el1_generic;
    hyp->cntv_ctl = 0;
    hyp->cpacr    = Cpu::Cpacr_el1_generic_hyp;
  }

  void load_host_regs(Mword tpidruro)
  {
    asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(tpidruro));
    asm volatile ("msr HCR_EL2, %x0"     : : "r"(Cpu::Hcr_host_bits));
  }

  [[gnu::nonnull]]
  void switch_to_guest(Context_hyp *hyp)
  {
    asm volatile ("msr VPIDR_EL2, %x0"  : : "r"(vpidr));
    asm volatile ("msr VMPIDR_EL2, %x0" : : "r"(vmpidr));
    hyp->cntvoff = cntvoff;
    asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(cntvoff));
    asm volatile ("msr CNTV_CTL_EL0, %x0": : "r"(guest_regs.cntv_ctl));
    asm volatile ("msr SCTLR_EL1, %x0"   : : "r"(guest_regs.sctlr));
    asm volatile ("msr CPACR_EL1, %x0"   : : "r"(guest_regs.cpacr));
    asm volatile ("msr CNTHCTL_EL2, %x0" : : "r"(Guest_cnthctl));
  }

  [[gnu::nonnull]]
  void switch_to_guest_no_load(Context_hyp *hyp)
  {
    sctlr = guest_regs.sctlr;
    hyp->cpacr = guest_regs.cpacr;
    hyp->cntv_ctl = guest_regs.cntv_ctl;
    hyp->cntvoff  = cntvoff;
  }

  static Mword load_guest_regs(Unsigned64 hcr, Mword tpidruro)
  {
    Mword old_tpidruro;
    asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(old_tpidruro));
    asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
    asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(tpidruro));
    return old_tpidruro;
  }

  static void load_non_vm_state()
  {
    asm volatile ("msr HCR_EL2, %x0"   : : "r"(Cpu::Hcr_non_vm_bits));
    asm volatile ("msr HSTR_EL2, %x0"  : : "r"(Cpu::Hstr_non_vm));
    // load normal SCTLR ...
    asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Cpu::Sctlr_el1_generic));
    // disable all debug exceptions for non-vms, if we want debug
    // exceptions into JDB we need either per-thread or a global
    // setting for this value.
    asm volatile ("msr MDSCR_EL1, %x0"    : : "r"(0UL));
    asm volatile ("msr CNTV_CTL_EL0, %x0" : : "r"(0UL)); // disable VTIMER
    // CNTKCTL: allow access to virtual and physical counter from PL0
    // see: generic_timer.cpp: setup_timer_access (Hyp)
    asm volatile("msr CNTKCTL_EL1, %x0"   : : "r"(0x3UL));
    asm volatile("msr CNTHCTL_EL2, %x0"   : : "r"(Host_cnthctl));
  }

  static void load_cnthctl(Unsigned64 cnthctl)
  {
    asm volatile ("msr CNTHCTL_EL2, %x0" : : "r"(cnthctl));
  }
};


inline
void
Context_hyp::save()
{
  asm volatile ("mrs %x0, PAR_EL1" : "=r"(par));
  asm volatile ("mrs %x0, HCR_EL2" : "=r"(hcr));

  // we do not save the CNTVOFF_EL2 because this kept in sync by the
  // VMM->VM switch code
  asm volatile ("mrs %x0, CNTV_CVAL_EL0"  : "=r"(cntv_cval));
  asm volatile ("mrs %x0, CNTKCTL_EL1"    : "=r"(cntkctl));
  asm volatile ("mrs %x0, CNTV_CTL_EL0"   : "=r"(cntv_ctl));
  asm volatile ("mrs %x0, TPIDR_EL1"      : "=r"(tpidrprw));
  asm volatile ("mrs %x0, CONTEXTIDR_EL1" : "=r"(contextidr));


  asm volatile ("mrs %x0, SP_EL1"    : "=r"(sp_el1));
  asm volatile ("mrs %x0, ELR_EL1"   : "=r"(elr_el1));
  asm volatile ("mrs %x0, VBAR_EL1"  : "=r"(vbar));
  asm volatile ("mrs %x0, CPACR_EL1" : "=r"(cpacr));

  asm volatile ("mrs %x0, SPSR_fiq"  : "=r"(spsr_fiq));
  asm volatile ("mrs %x0, SPSR_irq"  : "=r"(spsr_irq));
  asm volatile ("mrs %x0, SPSR_EL1"  : "=r"(spsr_svc));
  asm volatile ("mrs %x0, SPSR_abt"  : "=r"(spsr_abt));
  asm volatile ("mrs %x0, SPSR_und"  : "=r"(spsr_und));
  asm volatile ("mrs %x0, CSSELR_EL1": "=r"(csselr));
}

inline
void
Context_hyp::load()
{
  asm volatile ("msr PAR_EL1, %x0"        : : "r"(par));
  asm volatile ("msr HCR_EL2, %x0"        : : "r"(hcr));

  asm volatile ("msr CNTVOFF_EL2, %x0"    : : "r"(cntvoff));
  asm volatile ("msr CNTV_CVAL_EL0, %x0"  : : "r"(cntv_cval));
  asm volatile ("msr CNTKCTL_EL1, %x0"    : : "r"(cntkctl));
  asm volatile ("msr CNTV_CTL_EL0, %x0"   : : "r"(cntv_ctl));
  asm volatile ("msr TPIDR_EL1, %x0"      : : "r"(tpidrprw));
  asm volatile ("msr CONTEXTIDR_EL1, %x0" : : "r"(contextidr));

  asm volatile ("msr SP_EL1, %x0"         : : "r"(sp_el1));
  asm volatile ("msr ELR_EL1, %x0"        : : "r"(elr_el1));
  asm volatile ("msr VBAR_EL1, %x0"       : : "r"(vbar));
  asm volatile ("msr CPACR_EL1, %x0"      : : "r"(cpacr));

  asm volatile ("msr SPSR_fiq, %x0"       : : "r"(spsr_fiq));
  asm volatile ("msr SPSR_irq, %x0"       : : "r"(spsr_irq));
  asm volatile ("msr SPSR_EL1, %x0"       : : "r"(spsr_svc));
  asm volatile ("msr SPSR_abt, %x0"       : : "r"(spsr_abt));
  asm volatile ("msr SPSR_und, %x0"       : : "r"(spsr_und));
  asm volatile ("msr CSSELR_EL1, %x0"     : : "r"(csselr));
}

