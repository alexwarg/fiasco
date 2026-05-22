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
    Cp15_c1_mmu             = 1 << 0,
    Cp15_c1_alignment_check = 1 << 1,
    Cp15_c1_cache           = 1 << 2,
    Cp15_c1_branch_predict  = 1 << 11,
    Cp15_c1_v7_sw           = 1 << 10,
    Cp15_c1_insn_cache      = 1 << 12,
    Cp15_c1_high_vector     = 1 << 13,
  };

  enum : Mword
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
  };

#ifndef CONFIG_ARM_LPAE
  unsigned phys_bits() { return 32; }
#elif !defined(CONFIG_ARM_PT48) // CONFIG_ARM_LPAE && !CONFIG_ARM_PT48
  unsigned phys_bits() { return 40; }
#else // CONFIG_ARM_LPAE && CONFIG_ARM_PT48
  unsigned phys_bits() { return 48; }
#endif

};

class Cpu_arm_v5 : public Cpu_arm_generic
{
public:
  enum
  {
    Cp15_c1_write_buffer    = 1 << 3,
    Cp15_c1_prog32          = 1 << 4,
    Cp15_c1_data32          = 1 << 5,
    Cp15_c1_late_abort      = 1 << 6,
    Cp15_c1_big_endian      = 1 << 7,
    Cp15_c1_system_protect  = 1 << 8,
    Cp15_c1_rom_protect     = 1 << 9,
    Cp15_c1_f               = 1 << 10,
    Cp15_c1_rr              = 1 << 14,
    Cp15_c1_l4              = 1 << 15,

    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  Cp15_c1_alignment_check : 0)
                              | Cp15_c1_write_buffer
                              | Cp15_c1_prog32
                              | Cp15_c1_data32
                              | Cp15_c1_late_abort
                              | Cp15_c1_rom_protect
                              | Cp15_c1_high_vector,

    Cp15_c1_cache_bits      = Cp15_c1_cache
                              | Cp15_c1_insn_cache
                              | Cp15_c1_write_buffer,
  };
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
    Cp15_c1_ha              = 1 << 17,
    Cp15_c1_ee              = 1 << 25,
    Cp15_c1_nmfi            = 1 << 27,
    Cp15_c1_tre             = 1 << 28,
    Cp15_c1_te              = 1 << 30,
    Cp15_c1_rao_sbop        = (0xf << 3) | (1 << 16) | (1 << 18) | (1 << 22) | (1 << 23),

    Cp15_c1_cache_bits      = C::Cp15_c1_cache
                              | C::Cp15_c1_insn_cache,

    Cp15_c1_generic         = C::Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  C::Cp15_c1_alignment_check : 0)
                              | C::Cp15_c1_branch_predict
                              | C::Cp15_c1_high_vector
                              | Cp15_c1_tre
                              | Cp15_c1_rao_sbop,
  };
};

template<typename C, typename B>
class Cpu_arm_v7 : public Cpu_arm_v7plus<C, B>
{
public:
  enum Hstr_values
  {
    Hstr_non_vm = 0x39f6f, // ALL but crn=13,7 (TPIDxxR, DSB) CP15 trapped
    Hstr_vm = 0x0, // none
  };
};

template<typename C, typename B>
class Cpu_arm_v8 : public Cpu_arm_v7plus<C, B>
{
public:
  enum Hstr_values
  {
    Hstr_non_vm = 0x9f6f, // ALL but crn=13,7 (TPIDxxR, DSB) CP15 trapped
    Hstr_vm = 0x0, // none
  };
};

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
  static Cpu *_boot_cpu;
  Cpu_phys_id _phys_id;

public:
  Ids _cpu_id;
};
