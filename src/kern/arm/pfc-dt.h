#pragma once

#include <globalconfig.h>

#include <pfc-arm.h>

#ifdef CONFIG_ARM_PSCI
#include <pfc-psci.h>
using Pfc_dt_base = Pfc_psci;
#else
using Pfc_dt_base = Pfc_arm;
#endif

struct Pfc_dt : Pfc_dt_base
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override;
};
