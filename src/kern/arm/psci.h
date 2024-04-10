#pragma once

#include <types.h>
#include <globalconfig.h>
#include <smc_call.h>

#ifdef CONFIG_ARM_PSCI_SMC
#define FIASCO_ARM_PSCI_CALL_ASM_FUNC "smc #0"
#endif
#ifdef CONFIG_ARM_PSCI_HVC
#define FIASCO_ARM_PSCI_CALL_ASM_FUNC "hvc #0"
#endif

class Psci
{
#ifdef CONFIG_ARM_PSCI
public:
  static void init(Cpu_number cpu);

  struct Result
  {
    Mword res[4];
  };

  enum Error_codes
  {
    Psci_success            =  0,
    Psci_not_supported      = -1,
    Psci_invalid_parameters = -2,
    Psci_denied             = -3,
    Psci_already_on         = -4,
    Psci_on_pending         = -5,
    Psci_internal_failure   = -6,
    Psci_not_present        = -7,
    Psci_disabled           = -8,
    Psci_invalid_address    = -9,
  };

private:
  enum Functions
  {
    Psci_base_smc32          = 0x84000000,
    Psci_base_smc64          = 0xC4000000,

    Psci_version             = 0,
    Psci_cpu_suspend         = 1,
    Psci_cpu_off             = 2,
    Psci_cpu_on              = 3,
    Psci_affinity_info       = 4,
    Psci_migrate             = 5,
    Psci_migrate_info_type   = 6,
    Psci_migrate_info_up_cpu = 7,
    Psci_system_off          = 8,
    Psci_system_reset        = 9,
    Psci_features            = 10,
    Psci_cpu_freeze          = 11,
    Psci_cpu_default_suspend = 12,
    Psci_node_hw_state       = 13,
    Psci_system_suspend      = 14,
    Psci_set_suspend_mode    = 15,
    Psci_stat_residency      = 16,
    Psci_stat_count          = 17,
  };

  static bool is_v1;

  static unsigned long psci_fn(unsigned fn)
  {
    switch (fn)
      {
      case Psci_version:
      case Psci_cpu_off:
      case Psci_migrate_info_type:
      case Psci_system_off:
      case Psci_system_reset:
      case Psci_features:
      case Psci_cpu_freeze:
      case Psci_set_suspend_mode:
        return Psci_base_smc32 + fn;
      default:
        return (sizeof(long) == 8 ? Psci_base_smc64 : Psci_base_smc32) + fn;
      };
  }

public:
  static Psci::Result
  psci_call(Mword fn_id,
            Mword a0 = 0, Mword a1 = 0,
            Mword a2 = 0, Mword a3 = 0,
            Mword a4 = 0, Mword a5 = 0,
            Mword a6 = 0)
  {
    register Mword r0 FIASCO_ARM_ASM_REG(0) = psci_fn(fn_id);
    register Mword r1 FIASCO_ARM_ASM_REG(1) = a0;
    register Mword r2 FIASCO_ARM_ASM_REG(2) = a1;
    register Mword r3 FIASCO_ARM_ASM_REG(3) = a2;
    register Mword r4 FIASCO_ARM_ASM_REG(4) = a3;
    register Mword r5 FIASCO_ARM_ASM_REG(5) = a4;
    register Mword r6 FIASCO_ARM_ASM_REG(6) = a5;
    register Mword r7 FIASCO_ARM_ASM_REG(7) = a6;

    asm volatile(FIASCO_ARM_PSCI_CALL_ASM_FUNC
                 FIASCO_ARM_SMC_CALL_ASM_OPERANDS);

    Result res = { r0, r1, r2, r3 };
    return res;
  }

  static int cpu_on(unsigned long target, Address phys_tramp_mp_addr);
  static void system_reset();
  static void system_off();

#else // CONFIG_ARM_PSCI
public:
  static void init(Cpu_number)
  {}
#endif // CONFIG_ARM_PSCI
};

