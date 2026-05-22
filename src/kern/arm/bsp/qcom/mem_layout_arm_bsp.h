/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */
#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
#if defined(CONFIG_PF_QCOM_MSM8226) || defined(CONFIG_PF_QCOM_MSM8974)
  enum Phys_layout_qcom : Address
  {
    Gic_dist_phys_base = 0xf9000000,
    Gic_cpu_phys_base  = 0xf9002000,
    Gic_h_phys_base    = 0xf9001000,
    Gic_v_phys_base    = 0xf9004000,
    Mpm_ps_hold        = 0xfc4ab000,
  };
#endif

#if defined(CONFIG_PF_QCOM_MSM8909) || defined(CONFIG_PF_QCOM_MSM8916) || defined(CONFIG_PF_QCOM_MSM8939)
  enum Phys_layout_qcom : Address
  {
    Gic_dist_phys_base = 0x0b000000,
    Gic_cpu_phys_base  = 0x0b002000,
    Gic_h_phys_base    = 0x0b001000,
    Gic_v_phys_base    = 0x0b004000,
    Mpm_ps_hold        = 0x004ab000,
  };
#endif

#ifdef CONFIG_PF_QCOM_SM8150
  enum Phys_layout_qcom : Address
  {
    Gic_dist_phys_base   = 0x17a00000,
    Gic_redist_phys_base = 0x17a60000,
    Gic_redist_size      =   0x100000,
    Gic_its_phys_base    = 0x17a40000,
    Gic_its_size         =    0x20000,
  };
#endif
};
