#pragma once

#include <types.h>
#include <config.h>
#include "cpu_generic.h"
#include <paging.h>
#include <cpu_arm_defaults.h>
#ifdef CONFIG_ARM_EM_TZ
#include <panic.h>
#endif

class Cpu_arm_bits_generic : public Cpu_generic, public Cpu_arm_defaults
{
public:
#ifdef CONFIG_ARM_EM_TZ
  static char monitor_vector_base asm ("monitor_vector_base");
#endif

  static void modify_actrl(Mword set_mask, Mword clear_mask)
  {
    Mword t;
    asm volatile("mrc p15, 0, %[reg], c1, c0, 1 \n\t"
                 "bic %[reg], %[reg], %[clr]    \n\t"
                 "orr %[reg], %[reg], %[set]    \n\t"
                 "mcr p15, 0, %[reg], c1, c0, 1 \n\t"
                 : [reg] "=r" (t)
                 : [set] "r" (set_mask), [clr] "r" (clear_mask));
  }

  static void set_actrl(Mword bit_mask)
  { modify_actrl(bit_mask, 0); }

  static void clear_actrl(Mword bit_mask)
  { modify_actrl(0, bit_mask); }

  static Mword dfr1()
  { Mword v; asm volatile ("mrc p15, 0, %0, c0, c3, 5": "=r" (v)); return v; }

  static bool has_hpmn0()
  {
    if constexpr (IS_ENABLED(CONFIG_ARM_V8PLUS))
      return ((dfr1() >> 4) & 0xf) == 1;
    else
      return false;
  }

  static Mword midr() noexcept
  {
    Mword m;
    asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
    return m;
  }

  static Mword mpidr() noexcept
  {
    Mword mpid;
    asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpid));
    return mpid;
  }

#ifdef CONFIG_ARM_V7
  static Unsigned32 hcr()
  {
    Unsigned32 r;
    asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(r));
    return r;
  }

  static void hcr(Unsigned32 hcr)
  {
    asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(hcr));
  }
#endif // CONFIG_ARM_V7

#ifdef CONFIG_ARM_V8
  static Unsigned64 hcr()
  {
    Unsigned32 l, h;
    asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(l));
    asm volatile ("mrc p15, 4, %0, c1, c1, 4" : "=r"(h));
    return ((Unsigned64)h << 32) | l;
  }

  static void hcr(Unsigned64 hcr)
  {
    asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(hcr & 0xffffffff));
    asm volatile ("mcr p15, 4, %0, c1, c1, 4" : : "r"(hcr >> 32));
  }
#endif // CONFIG_ARM_V8
#ifndef CONFIG_CPU_VIRT
  void init_supervisor_mode(bool is_boot_cpu);
#endif
};

template<typename C, typename B>
class Cpu_arm_v6plus : public B
{
  C *self() noexcept { return static_cast<C *>(this); }
  C const *self() const noexcept { return static_cast<C const *>(this); }

public:
  void id_init()
  {
    __asm__("mrc p15, 0, %0, c0, c1, 0": "=r" (self()->_cpu_id._pfr[0]));
    __asm__("mrc p15, 0, %0, c0, c1, 1": "=r" (self()->_cpu_id._pfr[1]));
    __asm__("mrc p15, 0, %0, c0, c1, 2": "=r" (self()->_cpu_id._dfr0));
    __asm__("mrc p15, 0, %0, c0, c1, 3": "=r" (self()->_cpu_id._afr0));
    __asm__("mrc p15, 0, %0, c0, c1, 4": "=r" (self()->_cpu_id._mmfr[0]));
    __asm__("mrc p15, 0, %0, c0, c1, 5": "=r" (self()->_cpu_id._mmfr[1]));
    __asm__("mrc p15, 0, %0, c0, c1, 6": "=r" (self()->_cpu_id._mmfr[2]));
    __asm__("mrc p15, 0, %0, c0, c1, 7": "=r" (self()->_cpu_id._mmfr[3]));
  }

#if defined(CONFIG_ARM_CPU_ERRATA)
private:
  static void set_c15_c0_1(Mword bits_mask)
  {
    Mword t;
    asm volatile("mrc p15, 0, %0, c15, c0, 1 \n\t"
                 "orr %0, %0, %1             \n\t"
                 "mcr p15, 0, %0, c15, c0, 1 \n\t"
                 : "=r"(t) : "r" (bits_mask));
  }

public:
  static void init_errata_workarounds()
  {
    Mword mid = C::midr();

    if ((mid & 0xff000000) == 0x41000000) // ARM CPU
      {
        Mword rev = ((mid & 0x00f00000) >> 16) | (mid & 0x0f);
        Mword part = (mid & 0x0000fff0) >> 4;

        if (part == 0xc08) // Cortex A8
          {
            // errata: 430973
            if ((rev & 0xf0) == 0x10)
              C::set_actrl(1 << 6); // IBE to 1

            // errata: 458693
            if (rev == 0x20)
              C::set_actrl((1 << 5) | (1 << 9)); // L1NEON & PLDNOP

            // errata: 460075
            if (rev == 0x20)
              {
                Mword t;
                asm volatile ("mrc p15, 1, %0, c9, c0, 2 \n\t"
                              "orr %0, %0, #1 << 22      \n\t" // Write alloc disable
                              "mcr p15, 1, %0, c9, c0, 2 \n\t" : "=r"(t));
              }
          }

        if (part == 0xc09) // Cortex A9
          {
            // errata: 742230 (DMB errata)
            // make DMB a DSB to fix behavior
            if (rev <= 0x22) // <= r2p2
              set_c15_c0_1(1 << 4);

            // errata: 742231
            if (rev == 0x20 || rev == 0x21 || rev == 0x22)
              set_c15_c0_1((1 << 12) | (1 << 22));

            // errata: 743622
            if ((rev & 0xf0) == 0x20)
              set_c15_c0_1(1 << 6);

            // errata: 751472
            if (rev < 0x30)
              set_c15_c0_1(1 << 11);
          }
      }
  }

#endif
};


template<typename C, typename B>
class Cpu_arm_v6 : public Cpu_arm_v6plus<C, B>
{
  C *self() noexcept { return static_cast<C *>(this); }
  C const *self() const noexcept { return static_cast<C const *>(this); }

public:
  enum
  {
    Cp15_c1_l4              = 1 << 15,
    Cp15_c1_u               = 1 << 22,
    Cp15_c1_xp              = 1 << 23,
    Cp15_c1_ee              = 1 << 25,
    Cp15_c1_nmfi            = 1 << 27,
    Cp15_c1_tre             = 1 << 28,
    Cp15_c1_force_ap        = 1 << 29,

    Cp15_c1_cache_bits      = B::Cp15_c1_cache
                              | B::Cp15_c1_insn_cache,

#ifndef CONFIG_ARM_MPCORE
    Cp15_c1_generic         = B::Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  B::Cp15_c1_alignment_check : 0)
                              | B::Cp15_c1_branch_predict
                              | B::Cp15_c1_high_vector
                              | Cp15_c1_u
                              | Cp15_c1_xp,
#else // CONFIG_ARM_MPCORE
    Cp15_c1_generic         = B::Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check
                                 ? B::Cp15_c1_alignment_check : 0)
                              | B::Cp15_c1_branch_predict
                              | B::Cp15_c1_high_vector
                              | Cp15_c1_u
                              | Cp15_c1_xp
                              | Cp15_c1_tre,
#endif // CONFIG_ARM_MPCORE
  };

  static void modify_actrl(Mword set_mask, Mword clear_mask)
  {
    Mword t;
    asm volatile("mrc p15, 0, %[reg], c1, c0, 1 \n\t"
                 "bic %[reg], %[reg], %[clr]    \n\t"
                 "orr %[reg], %[reg], %[set]    \n\t"
                 "mcr p15, 0, %[reg], c1, c0, 1 \n\t"
                 : [reg] "=r" (t)
                 : [set] "r" (set_mask), [clr] "r" (clear_mask));
  }

  static void set_actrl(Mword bit_mask)
  { modify_actrl(bit_mask, 0); }

  static void clear_actrl(Mword bit_mask)
  { modify_actrl(0, bit_mask); }


  static void enable_smp() noexcept { set_actrl(0x20); }
  static void disable_smp() noexcept { clear_actrl(0x20); }
};

template<typename C, typename D>
class Cpu_arm_32 : public D
{
private:
  C *self() noexcept { return static_cast<C *>(this); }
  C const *self() const noexcept { return static_cast<C const *>(this); }

public:
  enum : Unsigned32
  {
    Hcr_must_set_bits = D::Hcr_vm | D::Hcr_swio | D::Hcr_ptw
                      | D::Hcr_amo | D::Hcr_imo | D::Hcr_fmo
                      | D::Hcr_tidcp | D::Hcr_tsc | D::Hcr_tactlr,

    /**
     * HCR value to be used for the VMM.
     *
     * The VMM runs in system mode (PL1), but has extended
     * CP15 access allowed.
     */
    Hcr_host_bits = Hcr_must_set_bits | D::Hcr_dc,

    /**
     * HCR value to be used for normal threads.
     *
     * On a hyp kernel all threads run per default in system mode (PL1).
     * However, all but the TPIDxyz CP15 accesses are disabled.
     */
    Hcr_non_vm_bits = Hcr_must_set_bits | D::Hcr_dc | D::Hcr_tsw
                      | D::Hcr_ttlb | D::Hcr_tvm
  };

  enum
  {
    Cp15_c1_cache_enabled  = D::Cp15_c1_generic | D::Cp15_c1_cache_bits,
    Cp15_c1_cache_disabled = D::Cp15_c1_generic,
  };

#ifndef CONFIG_ARM_LPAE
  static unsigned phys_bits() { return 32; }
#else
  static unsigned phys_bits() { return 40; }
#endif

  bool has_generic_timer() const { return (self()->_cpu_id._pfr[1] & 0xf0000) == 0x10000; }

  static Unsigned32 sctlr;

  static void check_for_swp_enable()
  {
    if (!Config::Cp15_c1_use_swp_enable)
      return;

    if (((D::midr() >> 16) & 0xf) != 0xf)
      return; // pre ARMv7 has no swap enable / disable

    Mword id_isar0;
    asm volatile ("mrc p15, 0, %0, c0, c2, 0" : "=r"(id_isar0));
    if ((id_isar0 & 0xf) != 1)
      return; // CPU has no swp / swpb

    if (((D::mpidr() >> 31) & 1) == 0)
      return; // CPU has no MP extensions -> no swp enable

    sctlr |= D::Cp15_c1_v7_sw;
  }

  static void enable_dcache() noexcept
  {
    asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
                 "orr     %0, %1                \n"
                 "mcr     p15, 0, %0, c1, c0, 0 \n"
                 : : "r" (0), "i" (D::Cp15_c1_cache));
  }

  static void disable_dcache() noexcept
  {
    asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
                 "bic     %0, %1                \n"
                 "mcr     p15, 0, %0, c1, c0, 0 \n"
                 : : "r" (0), "i" (D::Cp15_c1_cache));
  }

  static void early_init()
  {
    sctlr = Config::Cache_enabled
            ? Cp15_c1_cache_enabled : Cp15_c1_cache_disabled;

    check_for_swp_enable();

    // switch to supervisor mode and initialize the memory system
    asm volatile ( " mov  r2, r13             \n"
                   " mov  r3, r14             \n"
                   " msr  cpsr_c, %1          \n"
                   " mov  r13, r2             \n"
                   " mov  r14, r3             \n"

                   " mcr  p15, 0, %0, c1, c0  \n"
                   :
                   : "r" (sctlr),
                     "r" (Proc::Status_mode_supervisor
                          | Proc::Status_interrupts_disabled)
                   : "r2", "r3");

    Mem_unit::clean_dcache();
    C::enable_smp();
    Mem_unit::flush_cache();
  }

#ifdef CONFIG_ARM_EM_TZ
  static void init_tz()
  {
    Mword sctrl;
    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctrl));
    if (sctrl & D::Cp15_c1_nmfi)
      panic("Non-maskable FIQs (NMFI) detected, cannot use TZ mode");

    // set monitor vector base address
    assert(!(reinterpret_cast<Mword>(&D::monitor_vector_base) & 31));
    asm volatile ("mcr p15, 0, %0, c12, c0, 1" : : "r" (&D::monitor_vector_base));

    Mword dummy;
    asm volatile (
        "mov  %[dummy], sp \n"
        "cps  #0x16        \n"
        "mov  sp, %[dummy] \n"
        : [dummy] "=r" (dummy) : : "lr" );
    // running in monitor mode

    asm ("mcr  p15, 0, %[scr], c1, c1, 0" : : [scr] "r" (0x1));
    Mem::isb();

    asm ("mcr  p15, 0, %0, c12, c0, 0" : : "r" (0)); // reset VBAR
    asm ("mcr  p15, 0, %0, c13, c0, 0" : : "r" (0)); // reset FCSEIDR
    asm ("mcr  p15, 0, %0, c1, c0, 0"  : : "r" (0x4500a0));  // SCTLR = U | (18) | (16) | (7)

    asm ("mcr  p15, 0, %[scr], c1, c1, 0 \n" : : [scr] "r" (0x100));
    Mem::isb();
    asm volatile (
        "mov  %[dummy], sp \n"
        "cps  #0x13        \n"
        "mov  sp, %[dummy] \n"
        : [dummy] "=r" (dummy) : : "lr" );
    // running in svc mode


    // enable nonsecure access to vfp coprocessor
    asm volatile("mcr p15, 0, %0, c1, c1, 2" : : "r" (0xc00));

    enum
    {
      SCR_NS  = 1 << 0,
      SCR_IRQ = 1 << 1,
      SCR_FIQ = 1 << 2,
      SCR_EA  = 1 << 3,
      SCR_FW  = 1 << 4,
      SCR_AW  = 1 << 5,
      SCR_nET = 1 << 6,
      SCR_SCD = 1 << 7,
      SCR_HCE = 1 << 8,
      SCR_SIF = 1 << 9,
    };
  }

  static Mword tz_scr() noexcept
  {
    Mword r;
    asm volatile ("mrc p15, 0, %0, c1, c1, 0" : "=r" (r));
    return r;
  }

  static void tz_scr(Mword val) noexcept
  {
    asm volatile ("mcr p15, 0, %0, c1, c1, 0" : : "r" (val));
  }
#endif
#ifdef CONFIG_CPU_VIRT
  enum : Mword
  {
    Hdcr_bits = (IS_ENABLED(CONFIG_PERF_CNT_USER) ? (D::Mdcr_tpmcr | D::Mdcr_tpm) : 0)
                | D::Mdcr_tde
                | D::Mdcr_tda | D::Mdcr_tdosa | D::Mdcr_tdra | D::Mdcr_ttrf,
  };

  static void init_hyp_mode()
  {
    extern char hyp_vector_base[];

    assert (!(reinterpret_cast<Mword>(hyp_vector_base) & 31));
    asm volatile ("mcr p15, 4, %0, c12, c0, 0 \n" : : "r"(hyp_vector_base));

    asm volatile (
          "mcr p15, 4, %0, c2, c1, 2" // VTCR
          : : "r" ((1UL << 31) | (Page::Tcr_attribs << 8) | (1 << 6)));

    Mword sctlr_ignore;
    asm volatile (
          "mcr p15, 4, %[hdcr], c1, c1, 1 \n"     // HDCR
          "mrc p15, 0, %[sctlr], c1, c0, 0 \n"    // SCTLR
          "bic %[sctlr], #1 \n"                   // disable PL1&0 stage 1 MMU
          "mcr p15, 0, %[sctlr], c1, c0, 0 \n"    // SCTLR
          :
          [sctlr]"=&r"(sctlr_ignore)
          :
          [hdcr]"r"(Mword{Hdcr_bits} | (D::has_hpmn0() ? 0 : 1)));

    C::hcr(Hcr_non_vm_bits);
    asm ("mcr p15, 4, %0, c1, c1, 3" : : "r"(D::Hstr_non_vm)); // HSTR

    Mem::dsb();
    Mem::isb();

    // HCPTR
    asm volatile("mcr p15, 4, %0, c1, c1, 2" : :
                 "r" (  0x33ff       // TCP: 0-9, 12-13
                      | (1 << 20))); // TTA
  }
#endif
};


template<typename C, typename D>
Unsigned32 Cpu_arm_32<C, D>::sctlr;

template<typename C, typename D>
using Cpu_arm_bits = Cpu_arm_32<C, D>;
