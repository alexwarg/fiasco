#pragma once

#include <types.h>
#include <globalconfig.h>
#include <mem_unit.h>
#include <cpu_generic.h>
#include <cpu_arm_defaults.h>
#include <paging.h>
#include <panic.h>
#include <alternatives.h>

class Cpu_arm_bits_generic : public Cpu_generic, public Cpu_arm_defaults
{};

template<typename C, typename D>
class Cpu_arm_64 : public D
{
private:
  C *self() noexcept { return static_cast<C *>(this); }
  C const *self() const noexcept { return static_cast<C const *>(this); }

public:
  static constexpr Mword Scr_ns   = 1UL <<  0; ///< Non-Secure mode
  static constexpr Mword Scr_irq  = 1UL <<  1; ///< IRQ to EL3
  static constexpr Mword Scr_fiq  = 1UL <<  2; ///< FIQ to EL3
  static constexpr Mword Scr_ea   = 1UL <<  3; ///< External Abort and SError to EL3
  static constexpr Mword Scr_smd  = 1UL <<  7; ///< SMC disable
  static constexpr Mword Scr_hce  = 1UL <<  8; ///< HVC enable at EL1, EL2, and EL3
  static constexpr Mword Scr_sif  = 1UL <<  9; ///< Secure instruction fetch enable
  static constexpr Mword Scr_rw   = 1UL << 10; ///< EL2 / EL1 is AArch64
  static constexpr Mword Scr_st   = 1UL << 11; ///< Trap Secure EL1 access to timer to EL3
  static constexpr Mword Scr_twi  = 1UL << 12; ///< Trap WFI to EL3
  static constexpr Mword Scr_twe  = 1UL << 13; ///< Trap WFE to EL3
  static constexpr Mword Scr_apk  = 1UL << 16; ///< Do not trap on Pointer Authentication key accesses
  static constexpr Mword Scr_api  = 1UL << 17; ///< Do not trap on Pointer Authentication instructions
  static constexpr Mword Scr_eel2 = 1UL << 18; ///< Secure EL2 enable

  static constexpr Mword Sctlr_m       = 1UL << 0;
  static constexpr Mword Sctlr_a       = 1UL << 1;
  static constexpr Mword Sctlr_c       = 1UL << 2;
  static constexpr Mword Sctlr_sa      = 1UL << 3;
  static constexpr Mword Sctlr_sa0     = 1UL << 4;
  static constexpr Mword Sctlr_cp15ben = 1UL << 5;
  static constexpr Mword Sctlr_itd     = 1UL << 7;
  static constexpr Mword Sctlr_sed     = 1UL << 8;
  static constexpr Mword Sctlr_uma     = 1UL << 9;
  static constexpr Mword Sctlr_i       = 1UL << 12;
  static constexpr Mword Sctlr_dze     = 1UL << 14;
  static constexpr Mword Sctlr_uct     = 1UL << 15;
  static constexpr Mword Sctlr_ntwi    = 1UL << 16;
  static constexpr Mword Sctlr_ntwe    = 1UL << 18;
  static constexpr Mword Sctlr_wxn     = 1UL << 19;
  static constexpr Mword Sctlr_e0e     = 1UL << 24;
  static constexpr Mword Sctlr_ee      = 1UL << 25;
  static constexpr Mword Sctlr_uci     = 1UL << 26;

  static constexpr Mword Sctlr_el1_res = (1UL << 11) | (1UL << 20) | (3UL << 22) | (3UL << 28);

  static constexpr Mword Sctlr_el1_generic = Sctlr_c
                                             | Sctlr_cp15ben
                                             | Sctlr_i
                                             | Sctlr_dze
                                             | Sctlr_uct
                                             | Sctlr_uci
                                             | Sctlr_el1_res;

  static constexpr Mword Cptr_el2_generic    = 0x33ffUL; // Reserved(RES1): 0-9, 12-13
  static constexpr Mword Cptr_el2_tfp        = 1UL << 10; // Trap advanced SIMD and floating-point
  static constexpr Mword Cptr_el2_tta        = 1UL << 20; // Trap accesses to trace registers

  static constexpr Mword Cptr_el3_ez         = 1UL << 8; // Do not trap SVE instructions.

  static constexpr Mword Cptr_el2_tz         = 1UL << 8; // Trap SVE instructions.

  // Trap advanced SVE instructions at both EL0 and EL1.
  static constexpr Mword Cpacr_el1_zen_full  = 3UL << 16;

  // Trap advanced SIMD and floating-point instructions at both EL0 and EL1.
  static constexpr Mword Cpacr_el1_fpen_full = 3UL << 20;

  // When we run at EL2 we have to make sure that CPACR_EL1.FPEN is 3 when
  // user-mode runs with HCR.TGE = 1, otherwise we get undefined instruction
  // exceptions instead of FPU traps into EL2.
  static constexpr Mword Cpacr_el1_generic_hyp = Cpacr_el1_fpen_full
                            | (IS_ENABLED(CONFIG_ARM_SVE) ? Cpacr_el1_zen_full : 0);

  static constexpr Mword Zcr_vl_128  = 0;
  static constexpr Mword Zcr_vl_2048 = 15;
  static constexpr Mword Zcr_vl_max  = Zcr_vl_2048;
  static constexpr Mword Zcr_vl_mask = 0xf;

  struct boot_cpu_has_aarch32_el1
  : public Alternative_static_functor<boot_cpu_has_aarch32_el1>
  {
    static bool probe()
    {
      Mword pfr0;
      asm ("mrs %0, ID_AA64PFR0_EL1": "=r" (pfr0));
      return pfr0 & 0x20;
    }
  };

  struct boot_cpu_has_sme : public Alternative_static_functor<boot_cpu_has_sme>
  {
    static bool probe()
    {
      Mword pfr1;
      asm ("mrs %0, ID_AA64PFR1_EL1": "=r" (pfr1));
      return ((pfr1 >> 24) & 0xf) > 0;
    }
  };

  static bool has_ras()
  {
    Mword pfr0;
    asm ("mrs %0, ID_AA64PFR0_EL1": "=r" (pfr0));
    return (pfr0 >> 28) & 0xf;
  }

  static bool is_canonical_address(Address addr) noexcept
  {
    // cf. ARMv8-A Address Translation
    return addr >= 0xffff000000000000UL || addr <= 0x0000ffffffffffffUL;
  }

  bool has_sve() const
  { return ((self()->_cpu_id._pfr[0] >> 32) & 0xf) == 1; }

  static bool has_generic_timer() noexcept
  { return true; }

  bool has_hpmn0() const
  { return ((self()->_cpu_id._dfr0 >> 60) & 0xf) == 1; }

  bool has_pmuv3() const
  {
    unsigned pmuv = (self()->_cpu_id._dfr0 >> 8) & 0xf;
    return pmuv >= 1 && pmuv != 0xf;
  }

  bool has_pmuv2() const
  {
    unsigned pmuv = (self()->_cpu_id._dfr0 >> 8) & 0xf;
    return pmuv >= 1 && pmuv != 0xf;
  }

  static Mword midr() noexcept
  {
    Mword m;
    asm volatile ("mrs %0, midr_el1" : "=r" (m));
    return m;
  }

  static Mword mpidr() noexcept
  {
    Mword mpid;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpid));
    return mpid;
  }

  void id_init() noexcept
  {
    __asm__("mrs %0, ID_AA64PFR0_EL1": "=r" (self()->_cpu_id._pfr[0]));
    __asm__("mrs %0, ID_AA64PFR1_EL1": "=r" (self()->_cpu_id._pfr[1]));
    __asm__("mrs %0, S3_0_C0_C4_2": "=r" (self()->_cpu_id._pfr[2])); // ID_AA64PFR2_EL1
    __asm__("mrs %0, ID_AA64DFR0_EL1": "=r" (self()->_cpu_id._dfr0));
    __asm__("mrs %0, ID_AA64AFR0_EL1": "=r" (self()->_cpu_id._afr0));
    __asm__("mrs %0, ID_AA64MMFR0_EL1": "=r" (self()->_cpu_id._mmfr[0]));
    __asm__("mrs %0, ID_AA64MMFR1_EL1": "=r" (self()->_cpu_id._mmfr[1]));
    __asm__("mrs %0, ID_AA64MMFR2_EL1": "=r" (self()->_cpu_id._mmfr[2]));
    __asm__("mrs %0, S3_0_C0_C7_3" : "=r" (self()->_cpu_id._mmfr[3])); // ID_AA64MMFR3_EL1
    __asm__("mrs %0, S3_0_C0_C7_4" : "=r" (self()->_cpu_id._mmfr[4])); // ID_AA64MMFR4_EL1
  }

  static void early_init() noexcept
  {
    Mem_unit::clean_dcache();
    Mem_unit::flush_cache();
  }

  static void enable_smp() {}
  static void disable_smp() {}

  static Unsigned64 hcr() noexcept
  {
    Unsigned64 r;
    asm volatile ("mrs %0, HCR_EL2" : "=r"(r));
    return r;
  }

  static void hcr(Unsigned64 hcr) noexcept
  {
    asm volatile ("msr HCR_EL2, %0" : : "r"(hcr));
  }

  static unsigned pa_range()
  {
    Mword id_aa64mmfr0_el1;
    asm("mrs %0, S3_0_C0_C7_0" : "=r"(id_aa64mmfr0_el1));
    return id_aa64mmfr0_el1 & 0x0fU;
  }

  static unsigned phys_bits()
  {
    static char const pa_range_bits[16] = { 32, 36, 40, 42, 44, 48, 52 };
    return pa_range_bits[pa_range()];
  }

#ifndef CONFIG_CPU_VIRT
  static constexpr Mword
  Sctlr_generic = Sctlr_el1_generic
                  | Sctlr_m
                  | (Config::Sctlr_use_alignment_check ?  Sctlr_a : 0);

  static constexpr Unsigned64
  Scr_default_bits = Scr_ns | Scr_rw | Scr_smd;

  unsigned asid_bits() const noexcept
  { return (self()->_cpu_id._mmfr[0] & 0xf0U) == 0x20 ? 16 : 8; }

  void init_supervisor_mode(bool)
  {
    extern char exception_vector[];
    asm volatile ("msr VBAR_EL1, %0" : : "r"(&exception_vector));

    if (asid_bits() < Mem_unit::Asid_bits)
      panic("ASID size too small: HW provides %d bits, configured %d bits!",
            asid_bits(), Mem_unit::Asid_bits);
  }

  // SVE related registers
  static Mword zcr()
  {
    return zcr_el1();
  }

  static void zcr(Unsigned64 zcr)
  {
    return zcr_el1(zcr);
  }
#endif // !CONFIG_CPU_VIRT

#ifdef CONFIG_CPU_VIRT
  enum : Unsigned64
  {
    Scr_default_bits = Scr_ns | Scr_rw | Scr_smd | Scr_hce,
  };

  enum : Unsigned64
  {
    Hcr_must_set_bits = D::Hcr_vm | D::Hcr_swio | D::Hcr_ptw
                      | D::Hcr_amo | D::Hcr_imo | D::Hcr_fmo
                      | D::Hcr_tidcp | D::Hcr_tsc | D::Hcr_tactlr
                      | D::Hcr_tlor | D::Hcr_terr | D::Hcr_tea,
    /**
     * HCR value to be used for the VMM.
     *
     * The AArch64 VMM is currently running in EL1.
     */
    Hcr_host_bits = Hcr_must_set_bits | D::Hcr_rw | D::Hcr_dc,

    /**
     * HCR value to be used for normal threads.
     *
     * On AArch64 (with virtualization support) running in EL1.
     */
    Hcr_non_vm_bits = Hcr_must_set_bits | D::Hcr_rw | D::Hcr_dc | D::Hcr_tsw
                      | D::Hcr_ttlb | D::Hcr_tvm | D::Hcr_trvm
  };

  enum
  {
    Sctlr_res = (3UL << 4)  | (1UL << 11) | (1UL << 16)
              | (1UL << 18) | (3UL << 22) | (3UL << 28),

    Sctlr_generic = Sctlr_m
                    | (Config::Sctlr_use_alignment_check ?  Sctlr_a : 0)
                    | Sctlr_c
                    | Sctlr_i
                    | Sctlr_res,
  };

  enum : Mword
  {
    Mdcr_bits      = (IS_ENABLED(CONFIG_PERF_CNT_USER) ? (D::Mdcr_tpmcr | D::Mdcr_tpm) : 0)
                     | D::Mdcr_tda | D::Mdcr_tdosa | D::Mdcr_tdra | D::Mdcr_tpms | D::Mdcr_ttrf,
    Mdcr_vm_mask   = 0xf00,
  };

  unsigned vmid_bits() const noexcept
  { return (self()->_cpu_id._mmfr[1] & 0xf0U) == 0x20 ? 16 : 8; }

  void init_hyp_mode(bool is_boot_cpu) noexcept
  {
    extern char exception_vector[];

    C::init_ras(is_boot_cpu);

    if (vmid_bits() < Mem_unit::Asid_bits)
      panic("VMID size too small: HW provides %d bits, configured %d bits!",
            vmid_bits(), Mem_unit::Asid_bits);

    asm volatile ("msr VBAR_EL2, %x0" : : "r"(&exception_vector));
    asm volatile ("msr VTCR_EL2, %x0" : :
                  "r"(  (1UL << 31) // RES1
                      | (Page::Tcr_attribs << 8)
                      | Page::vtcr_bits(pa_range())));

    Mword mdcr;
    if (self()->has_pmuv3())
      {
        Mword pmcr;
        asm ("mrs %0, PMCR_EL0" : "=r"(pmcr));
        mdcr = (pmcr >> 11) & 0x1f;
      }
    else
      {
        asm ("mrs %0, MDCR_EL2" : "=r"(mdcr));
        mdcr &= 0x1f; // keep HPMN reset value
      }
    mdcr |= Mdcr_bits;
    asm volatile ("msr MDCR_EL2, %x0" : : "r"(mdcr));

    asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Sctlr_el1_generic));
    hcr(Hcr_non_vm_bits);
    asm volatile ("msr HSTR_EL2, %x0" : : "r" (D::Hstr_non_vm));

    Mem::dsb();
    Mem::isb();

    // HCPTR
    asm volatile("msr CPTR_EL2, %x0" : : "r" (Cptr_el2_generic | Cptr_el2_tta));
  }

  // SVE related registers
  static Mword zcr()
  {
    Unsigned64 r;
    asm volatile (".arch_extension sve    \n"
                  "mrs %0, ZCR_EL2        \n"
                  ".arch_extension nosve  \n"
                 : "=r"(r));
    return r;
  }

  static void zcr(Unsigned64 zcr)
  {
    asm volatile (".arch_extension sve    \n"
                  "msr ZCR_EL2, %0        \n"
                  ".arch_extension nosve  \n"
                  : : "r"(zcr));
  }

#endif // CONFIG_CPU_VIRT

  // SVE related registers
  static unsigned sve_vl()
  {
    Mword vl;
    asm volatile (".arch_extension sve    \n"
                  "rdvl %0, #1            \n"
                  ".arch_extension nosve  \n"
                  : "=r"(vl));
    // rdvl returns the vector length in bytes, but we measure the vector length
    // in quad-words (128-bits).
    return vl / 16;
  }

  static Mword zcr_el1()
  {
    Unsigned64 r;
    asm volatile (".arch_extension sve    \n"
                  "mrs %0, ZCR_EL1        \n"
                  ".arch_extension nosve  \n"
                  : "=r"(r));
    return r;
  }

  static void zcr_el1(Unsigned64 zcr)
  {
    asm volatile (".arch_extension sve    \n"
                  "msr ZCR_EL1, %0        \n"
                  ".arch_extension nosve  \n"
                  : : "r"(zcr));
  }


};

template<typename C, typename B>
class Cpu_arm_v6plus : public B
{};

template<typename C, typename D>
using Cpu_arm_bits = Cpu_arm_64<C, D>;
