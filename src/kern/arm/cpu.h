#pragma once

#include "cpu_generic.h"
#include "io.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "types.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "cpu-arm-bits.h"

#ifdef CONFIG_JDB
#include <stdio.h>
#endif

#if defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_CORTEX_A9) || defined(CONFIG_ARM_CORTEX_A5)
#include <scu.h>
#endif

class Cpu_arm_generic : public Cpu_arm_bits_generic
{
public:
  enum
  {
    Sctlr_m     = 1 << 0,
    Sctlr_a     = 1 << 1,
    Sctlr_c     = 1 << 2,
    Sctlr_z     = 1 << 11,
    Sctlr_v7_sw = 1 << 10,
    Sctlr_i     = 1 << 12,
    Sctlr_v     = 1 << 13,
  };

  enum : Unsigned64
  {
    Hcr_vm     = 1UL << 0,  ///< Virtualization enable
    Hcr_swio   = 1UL << 1,  ///< Set/way invalidation override
    Hcr_ptw    = 1UL << 2,  ///< Protected table walk
    Hcr_fmo    = 1UL << 3,  ///< Physical FIQ routing
    Hcr_imo    = 1UL << 4,  ///< Physical IRQ routing
    Hcr_amo    = 1UL << 5,  ///< Physical SError interrupt routing
    Hcr_dc     = 1UL << 12, ///< Default cacheability
    Hcr_tid2   = 1UL << 17, ///< Trap CTR, CESSLR, etc.
    Hcr_tid3   = 1UL << 18, ///< Trap ID, etc.
    Hcr_tsc    = 1UL << 19, ///< Trap SMC instructions
    Hcr_tidcp  = 1UL << 20, ///< Trap implementation defined functionality
    Hcr_tactlr = 1UL << 21, ///< Trap ACTLR, etc.
    Hcr_tsw    = 1UL << 22, ///< Trap cache maintenance instructions
    Hcr_ttlb   = 1UL << 25, ///< Trap TLB maintenance instructions
    Hcr_tvm    = 1UL << 26, ///< Trap virtual memory controls
    Hcr_tge    = 1UL << 27, ///< Trap General Exceptions
    Hcr_hcd    = 1UL << 29, ///< HVC instruction disable
    Hcr_trvm   = 1UL << 30, ///< Trap reads of virtual memory controls
    Hcr_rw     = 1UL << 31, ///< EL1 is AArch64
    Hcr_tlor   = 1ULL << 35, ///< LOR: Trap FEAT_LOR registers, not def for HCR2
    Hcr_terr   = 1ULL << 36, ///< RAS: Trap FEAT_RAS registers
    Hcr_tea    = 1ULL << 37, ///< RAS: Route Ext Abort EL0/EL1 exceptions to EL2
  };

  enum : Mword
  {
    // HDCR[31:0] (arm32) is architecturally mapped to MDCR_EL2[31:0] (arm64).
    Mdcr_hpmn_mask = 0xf,
    Mdcr_tpmcr     = 1UL << 5,
    Mdcr_tpm       = 1UL << 6,
    Mdcr_hpme      = 1UL << 7,
    Mdcr_tde       = 1UL << 8,
    Mdcr_tda       = 1UL << 9,
    Mdcr_tdosa     = 1UL << 10,
    Mdcr_tdra      = 1UL << 11,
    Mdcr_tpms      = 1UL << 14,
    Mdcr_ttrf      = 1UL << 19,
  };

  static constexpr bool has_pmuv3() { return false; }
};

class Cpu_arm_v5 : public Cpu_arm_generic
{
public:
  enum
  {
    Sctlr_write_buffer    = 1 << 3,
    Sctlr_prog32          = 1 << 4,
    Sctlr_data32          = 1 << 5,
    Sctlr_late_abort      = 1 << 6,
    Sctlr_big_endian      = 1 << 7,
    Sctlr_system_protect  = 1 << 8,
    Sctlr_rom_protect     = 1 << 9,
    Sctlr_f               = 1 << 10,
    Sctlr_rr              = 1 << 14,
    Sctlr_l4              = 1 << 15,
  };

  static constexpr Unsigned32 Sctlr_cache_bits =
                                Sctlr_c
                                | Sctlr_i
                                | Sctlr_write_buffer;

  static constexpr Unsigned32 Sctlr_generic =
                                Sctlr_m
                                | (Config::Sctlr_use_alignment_check ?  Sctlr_a : 0)
                                | (Config::Cache_enabled
                                   ? Sctlr_cache_bits : 0)
                                | Sctlr_write_buffer
                                | Sctlr_prog32
                                | Sctlr_data32
                                | Sctlr_late_abort
                                | Sctlr_rom_protect
                                | Sctlr_v;
};


class Cpu_arm_v7plus_common
{
public:
  static void enable_smp();
  static void disable_smp();
};

template<typename C, typename B>
class Cpu_arm_v7plus : public Cpu_arm_v6plus<C, B>, public Cpu_arm_v7plus_common
{
public:
  using Cpu_arm_v7plus_common::enable_smp;
  using Cpu_arm_v7plus_common::disable_smp;

  enum
  {
    Sctlr_ha              = 1 << 17,
    Sctlr_ee              = 1 << 25,
    Sctlr_nmfi            = 1 << 27,
    Sctlr_tre             = 1 << 28,
    Sctlr_te              = 1 << 30,
    Sctlr_rao_sbop        = (0xf << 3) | (1 << 16) | (1 << 18) | (1 << 22) | (1 << 23),
  };

  static constexpr Unsigned32 Sctlr_cache_bits =
                                C::Sctlr_c | C::Sctlr_i;

  static constexpr Unsigned32 Sctlr_generic =
                                C::Sctlr_m
                                | (Config::Sctlr_use_alignment_check
                                   ? C::Sctlr_a : 0)
                                | (Config::Cache_enabled
                                   ? Sctlr_cache_bits : 0)
                                | C::Sctlr_z
                                | C::Sctlr_v
                                | C::Sctlr_tre
                                | C::Sctlr_rao_sbop;

  enum Hstr_values
  {
    Hstr_non_vm = (1 << 0)
                | (1 << 1)
                | (1 << 2)
                | (1 << 3)
                | (0 << 4) // res0
                | (1 << 5)
                | (1 << 6)
                | (0 << 7) // enable data and instruction barrier operations
                | (1 << 8)
                | ((IS_ENABLED(CONFIG_PERF_CNT_USER) ? 0 : 1) << 9) // PMCCNTR
                | (1 << 10)
                | (1 << 11)
                | (1 << 12)
                | (0 << 13) // enable access to TPIDRxxR
                | (0 << 14) // res0
                | (1 << 15)
                | ((IS_ENABLED(CONFIG_ARM_V7) ? 1 : 0) << 16) // TTEE, only ARMv7
                | ((IS_ENABLED(CONFIG_ARM_V7) ? 1 : 0) << 17) // TJDBX, only ARMv7
                ,
    Hstr_vm = 0x0, // none
  };
};

template<typename C, typename B>
class Cpu_arm_v7 : public Cpu_arm_v7plus<C, B>
{};

template<typename C, typename B>
class Cpu_arm_v8 : public Cpu_arm_v7plus<C, B>
{};

class Cpu;

#if defined(CONFIG_ARM_V5)
using Cpu_arm = Cpu_arm_v5;
#elif defined(CONFIG_ARM_V6)
using Cpu_arm = Cpu_arm_v6<Cpu, Cpu_arm_generic>;
#elif defined(CONFIG_ARM_V7)
using Cpu_arm = Cpu_arm_v7<Cpu, Cpu_arm_generic>;
#elif defined(CONFIG_ARM_V8)
using Cpu_arm = Cpu_arm_v8<Cpu, Cpu_arm_generic>;
#else
#endif

class Cpu : public Cpu_arm_bits<Cpu, Cpu_arm>
{
  friend Cpu_arm_bits<Cpu, Cpu_arm>;

public:
  void init(bool resume, bool is_boot_cpu);

  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  Cpu(Cpu_number id) { set_id(id); }

  struct Ids
  {
    Mword _pfr[2], _dfr0, _afr0, _mmfr[4];
  };

  enum {
    Copro_dbg_model_not_supported = 0,
    Copro_dbg_model_v6            = 2,
    Copro_dbg_model_v6_1          = 3,
    Copro_dbg_model_v7            = 4,
    Copro_dbg_model_v7_1          = 5,
    Copro_dbg_model_v8            = 6,
    Copro_dbg_model_v8_plus_vhe   = 7,
    Copro_dbg_model_v8_2          = 8,
    Copro_dbg_model_v8_4          = 9,
  };


  unsigned copro_dbg_model() const { return _cpu_id._dfr0 & 0xf; }

#if defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_CORTEX_A9) || defined(CONFIG_ARM_CORTEX_A5)
  static Scu scu;
#endif

  static Mword stack_align(Mword stack) noexcept { return stack & ~0x3; }
  static bool have_superpages() { return true; }
  static void debugctl_enable() {}
  static void debugctl_disable() {}
  static Unsigned32 get_scaler_tsc_to_ns() { return 0; }
  static Unsigned32 get_scaler_tsc_to_us() { return 0; }
  static Unsigned32 get_scaler_ns_to_tsc() { return 0; }
  static bool tsc() { return 0; }
  static Unsigned64 rdtsc() { return 0; }
  Cpu_phys_id phys_id() const noexcept { return _phys_id; }

#ifndef CONFIG_JDB
  static void print_infos() {}
#else
  void print_infos() const noexcept
  {
    printf("Cache config: %s\n", Config::Cache_enabled ? "ON" : "OFF");
    printf("ID_PFR[01]:  %08lx %08lx", _cpu_id._pfr[0], _cpu_id._pfr[1]);
    printf(" ID_[DA]FR0: %08lx %08lx\n", _cpu_id._dfr0, _cpu_id._afr0);
    printf("ID_MMFR[04]: %08lx %08lx %08lx %08lx\n",
           _cpu_id._mmfr[0], _cpu_id._mmfr[1], _cpu_id._mmfr[2], _cpu_id._mmfr[3]);
  }
#endif

private:
  static void init_ras(bool is_boot_cpu);
  static Cpu *_boot_cpu;

  // 32 bits: 24..31: Aff3 (0 for ARM32); 16..23: Aff2; 8..15: Aff1; 0..7: Aff0
  Cpu_phys_id _phys_id;

public:
  Ids _cpu_id;
};
