#pragma once

#include "hyp_vm_state_generic.h"

#include "cpu.h"

struct Context_hyp : Context_hyp_generic
{
public:
  // Banked registers for irq, svc, abt, and und modes
  struct Banked_mode_regs
  {
    Mword sp, lr, spsr;
  };

  // Banked registers for fiq mode
  struct Banked_fiq_regs
  {
    Mword r8, r9, r10, r11, r12, sp, lr, spsr;
  };

  // we need to store all banked registers for PL1 modes
  // because a hyp kernel runs applications in system mode (PL1)
  Banked_fiq_regs fiq;
  Banked_mode_regs irq, svc, abt, und;

  void save();
  void load();

  [[gnu::nonnull]]
  void sanitize_psr(Mword *psr) const
  {
    if (hcr & Cpu::Hcr_tge)
    {
      // Must run in user mode if HCR.TGE is set. Otherwise the behaviour
      // is unpredictable (Armv7) or leads to an illegal exception return
      // (Armv8).
      *psr = (*psr & ~Proc::Status_mode_mask) | Proc::PSR_m_usr;
    }
  else
    {
      // allow all but hyp or mon mode
      Unsigned32 const forbidden = ~0x888f0000U;
      if ((1UL << (*psr & Proc::Status_mode_mask)) & forbidden)
        *psr = (*psr & ~Proc::Status_mode_mask) | Proc::PSR_m_sys;
    }
  }

};

class Hyp_vm_state : public Hyp_vm_state_generic
{
public:
  struct Regs_g
  {
    Unsigned64 hcr;

    Unsigned64 ttbr0;
    Unsigned64 ttbr1;
    Unsigned32 ttbcr;
    Unsigned32 sctlr;
    Unsigned32 dacr;
    Unsigned32 fcseidr;
    Unsigned32 cntv_ctl;
    Unsigned32 _res;
  };

  struct Regs_h
  {
    Unsigned64 hcr;
  };

  typedef Gic_h::Arm_vgic Gic;

  /* The following part is our user API */
  Regs_h host_regs;
  Regs_g guest_regs;

  Unsigned64 cntvoff;

  Unsigned32 vmpidr;
  Unsigned32 vpidr;

  Unsigned32 _res0[2];

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
  Unsigned32 csselr;

  Unsigned32 sctlr;
  Unsigned32 actlr;
  Unsigned32 cpacr;

  Unsigned64 ttbr0;
  Unsigned64 ttbr1;
  Unsigned32 ttbcr;

  Unsigned32 dacr;

  Unsigned32 dfsr;
  Unsigned32 ifsr;
  Unsigned32 adfsr;
  Unsigned32 aifsr;

  Unsigned32 dfar;
  Unsigned32 ifar;

  Unsigned32 mair0;
  Unsigned32 mair1;

  Unsigned32 amair0;
  Unsigned32 amair1;

  Unsigned32 vbar;

  Unsigned32 fcseidr;

  Unsigned32 fpinst;
  Unsigned32 fpinst2;

  void save()
  {
    // save vm state
    asm volatile ("mrc p15, 2, %0, c0, c0, 0" : "=r"(csselr));

    asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    // we unconditionally trap actlr accesses
    // asm ("mrc p15, 0, %0, c1, c0, 1" : "=r"(v->actlr));
    asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r"(cpacr));

    asm volatile ("mrrc p15, 0, %Q0, %R0, c2" : "=r"(ttbr0));
    asm volatile ("mrrc p15, 1, %Q0, %R0, c2" : "=r"(ttbr1));
    asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(ttbcr));

    asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r"(dacr));

    asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r"(dfsr));
    asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r"(ifsr));
    asm volatile ("mrc p15, 0, %0, c5, c1, 0" : "=r"(adfsr));
    asm volatile ("mrc p15, 0, %0, c5, c1, 1" : "=r"(aifsr));

    asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));
    asm volatile ("mrc p15, 0, %0, c6, c0, 2" : "=r"(ifar));

    asm volatile ("mrc p15, 0, %0, c10, c2, 0" : "=r"(mair0));
    asm volatile ("mrc p15, 0, %0, c10, c2, 1" : "=r"(mair1));

    asm volatile ("mrc p15, 0, %0, c10, c3, 0" : "=r"(amair0));
    asm volatile ("mrc p15, 0, %0, c10, c3, 1" : "=r"(amair1));

    asm volatile ("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));

    asm volatile ("mrc p15, 0, %0, c13, c0, 0" : "=r"(fcseidr));
  }

  void load(bool el0_only) const
  {
    asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_vm)); // HSTR
    asm volatile ("mcr p15, 2, %0, c0, c0, 0" : : "r"(csselr));

    Unsigned32 _sctlr = access_once(&sctlr);
    if (el0_only)
      _sctlr &= ~Cpu::Cp15_c1_mmu;

    asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(_sctlr));
    // we unconditionally trap actlr accesses
    // asm ("mcr p15, 0, %0, c1, c0, 1" : : "r"(v->actlr));
    asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r"(cpacr));

    asm volatile ("mcrr p15, 0, %Q0, %R0, c2" : : "r"(ttbr0));
    asm volatile ("mcrr p15, 1, %Q0, %R0, c2" : : "r"(ttbr1));
    asm volatile ("mcr p15, 0, %0, c2, c0, 2" : : "r"(ttbcr));

    asm volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr));

    asm volatile ("mcr p15, 0, %0, c5, c0, 0" : : "r"(dfsr));
    asm volatile ("mcr p15, 0, %0, c5, c0, 1" : : "r"(ifsr));
    asm volatile ("mcr p15, 0, %0, c5, c1, 0" : : "r"(adfsr));
    asm volatile ("mcr p15, 0, %0, c5, c1, 1" : : "r"(aifsr));

    asm volatile ("mcr p15, 0, %0, c6, c0, 0" : : "r"(dfar));
    asm volatile ("mcr p15, 0, %0, c6, c0, 2" : : "r"(ifar));

    asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(mair0));
    asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(mair1));

    asm volatile ("mcr p15, 0, %0, c10, c3, 0" : : "r"(amair0));
    asm volatile ("mcr p15, 0, %0, c10, c3, 1" : : "r"(amair1));

    asm volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r"(vbar));

    asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(fcseidr));

    asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (vmpidr));
    asm volatile ("mcr  p15, 4, %0, c0, c0, 0" : : "r" (vpidr));
  }

  static Unsigned32 arm_host_sctlr()
  {
    return (Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~(Cpu::Cp15_c1_mmu | (1 << 28));
  }

  void switch_to_host(Mword tpidruro)
  {
    asm volatile ("mrc p15, 0, %0, c13, c0, 3"
                  : "=r"(tpidruro));
    asm volatile ("mrc p15, 0, %0, c1,  c0, 0"
                  : "=r"(guest_regs.sctlr));
    asm volatile ("mrc p15, 0, %0, c13, c0, 0"
                  : "=r"(guest_regs.fcseidr));

    // fcse not supported in vmm
    asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(0));
    asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(arm_host_sctlr()));

    asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (guest_regs.cntv_ctl));
    // disable VTIMER
    asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0)); // CNTV_CTL
    asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r"(0ULL)); // CNTVOFF
  }

  [[gnu::nonnull]]
  void switch_to_host_no_load(Context_hyp *hyp)
  {
    guest_regs.sctlr      = sctlr;
    guest_regs.fcseidr    = fcseidr;
    guest_regs.cntv_ctl   = hyp->cntv_ctl;

    sctlr      = arm_host_sctlr();
    fcseidr    = 0;
    hyp->cntv_ctl = 0;
  }

  void load_host_regs(Mword tpidruro) const
  {
    asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(tpidruro));
    asm volatile ("mcr p15, 4, %0, c1,  c1, 0" : : "r"(Cpu::Hcr_host_bits));
  }

  [[gnu::nonnull]]
  void switch_to_guest(Context_hyp *hyp) const
  {
    asm volatile ("mcr p15, 0, %0, c13, c0, 0"
                  : : "r"(guest_regs.fcseidr));

    asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r" (guest_regs.sctlr));
    asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (guest_regs.cntv_ctl));
    hyp->cntvoff = cntvoff;
    asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r" (cntvoff));

    asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (vmpidr));
    asm volatile ("mcr  p15, 4, %0, c0, c0, 0" : : "r" (vpidr));
  }

  [[gnu::nonnull]]
  void switch_to_guest_no_load(Context_hyp *hyp)
  {
    fcseidr    = guest_regs.fcseidr;
    sctlr      = guest_regs.sctlr;
    hyp->cntv_ctl = guest_regs.cntv_ctl;
    hyp->cntvoff  = cntvoff;
  }

  static Mword load_guest_regs(Unsigned64 hcr, Mword tpidruro)
  {
    Mword old_tpidruro;
    asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(old_tpidruro));
    Cpu::hcr(hcr);
    asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(tpidruro));
    return old_tpidruro;
  }

  static void load_non_vm_state()
  {
    asm volatile ("mcr p15, 4, %0, c1, c1, 0"
                  : : "r"(Cpu::Hcr_non_vm_bits));
    asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_non_vm)); // HSTR
    // load normal SCTLR ...
    asm volatile ("mcr p15, 0, %0, c1, c0, 0"
                  : : "r" ((Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~Cpu::Cp15_c1_mmu));
    asm volatile ("mcr p15, 0, %0,  c1, c0, 2" : : "r" (0xf00000));
    asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r" (0));
  }
};

inline
void
Context_hyp::save()
{
  asm volatile ("mrrc p15, 0, %Q0, %R0, c7" : "=r"(par));
  hcr = Cpu::hcr();
  // we do not save the CNTVOFF_EL2 because this kept in sync by the
  // VMM->VM switch code
  asm volatile ("mrrc p15, 3, %Q0, %R0, c14" : "=r" (cntv_cval));
  asm volatile ("mrc p15, 0, %0, c14, c1, 0" : "=r" (cntkctl));
  asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (cntv_ctl));

  asm volatile ("mrc p15, 0, %0, c13, c0, 4" : "=r"(tpidrprw));
  asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r"(contextidr));

#define STORER(m, r) asm volatile ("mrs %0, "#r"_"#m : "=r"(m.r))
#define STOREX(m) do { STORER(m, sp); STORER(m, lr); STORER(m, spsr); } while(0)
  STORER(fiq, r8);
  STORER(fiq, r9);
  STORER(fiq, r10);
  STORER(fiq, r11);
  STORER(fiq, r12);
  STOREX(fiq);
  STOREX(irq);
  STOREX(svc);
  STOREX(abt);
  STOREX(und);
#undef STOREX
#undef STORER
}

inline
void
Context_hyp::load()
{
  asm volatile ("mcrr p15, 0, %Q0, %R0, c7" : : "r"(par));
  Cpu::hcr(hcr);
  asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r" (cntvoff));
  asm volatile ("mcrr p15, 3, %Q0, %R0, c14" : : "r" (cntv_cval));
  asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r" (cntkctl));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (cntv_ctl));

  asm volatile ("mcr p15, 0, %0, c13, c0, 4" : : "r"(tpidrprw));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r"(contextidr));

#define LOADR(m , r) asm volatile ("msr "#r"_"#m ", %0" : : "r"(m.r))
#define LOADX(m) do { LOADR(m, sp); LOADR(m, lr); LOADR(m, spsr); } while(0)
  LOADR(fiq, r8);
  LOADR(fiq, r9);
  LOADR(fiq, r10);
  LOADR(fiq, r11);
  LOADR(fiq, r12);
  LOADX(fiq);
  LOADX(irq);
  LOADX(svc);
  LOADX(abt);
  LOADX(und);
#undef LOADX
#undef LOADR
}
