#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
  enum Phys_layout_realview_all : Address {
    Flush_area_phys_base = 0xe0000000,
  };

#if defined(CONFIG_PF_REALVIEW_EB) || defined(CONFIG_PF_REALVIEW_PB11MP) \
    || defined(CONFIG_PF_REALVIEW_PBX) || defined(CONFIG_PF_REALVIEW_VEXPRESS_A9)
  enum Phys_layout_realview : Address {
    Devices0_phys_base    = 0x10000000,
    System_regs_phys_base = Devices0_phys_base,
    System_ctrl_phys_base = Devices0_phys_base + 0x00001000,
    Timer0_phys_base      = Devices0_phys_base + 0x00011000,
  };
#endif

#if defined(CONFIG_PF_REALVIEW_EB) \
    && !defined(CONFIG_ARM_MPCORE) && !defined(CONFIG_ARM_CORTEX_A9)
  enum Phys_layout_realview_single : Address {
    Gic_cpu_phys_base    = Devices0_phys_base  + 0x00040000,
    Gic_dist_phys_base   = Gic_cpu_phys_base   + 0x00001000,
  };
#endif

#if defined(CONFIG_PF_REALVIEW_EB) \
    && (defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_CORTEX_A9))
  enum Phys_layout_realview_mp : Address {
    Gic1_cpu_phys_base    = Devices0_phys_base + 0x00040000,
    Gic1_dist_phys_base   = Devices0_phys_base + 0x00041000,

    Devices1_phys_base   = 0x10100000,

    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,
  };
#endif

#ifdef CONFIG_PF_REALVIEW_PB11MP
  enum Phys_layout_realview_pb11mp : Address {
    Devices1_phys_base   = 0x1f000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,

    Devices2_phys_base   = 0x1e000000,
    Gic1_cpu_phys_base    = Devices2_phys_base,
    Gic1_dist_phys_base   = Devices2_phys_base + 0x00001000,
  };
#endif

#ifdef CONFIG_PF_REALVIEW_PBX
  enum Phys_layout_realview_pbx : Address {
    Devices1_phys_base    = 0x1f000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,

    Devices2_phys_base    = 0x1e000000,
    Gic2_cpu_phys_base    = Devices2_phys_base + 0x00020000,
    Gic2_dist_phys_base   = Devices2_phys_base + 0x00021000,
    Gic3_cpu_phys_base    = Devices2_phys_base + 0x00030000,
    Gic3_dist_phys_base   = Devices2_phys_base + 0x00031000,
  };
#endif

#ifdef CONFIG_PF_REALVIEW_VEXPRESS_A9
  enum Phys_layout_realview_vexpress_a9 : Address {
    Devices1_phys_base   = 0x1e000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,
  };
#endif

#if defined(CONFIG_PF_REALVIEW_VEXPRESS) && !defined(CONFIG_PF_REALVIEW_VEXPRESS_A9)
  enum Phys_layout_realview_vexpress_a15 {
    Devices0_phys_base   = 0x1c000000,
    System_regs_phys_base = 0x1c010000,
    System_ctrl_phys_base = 0x1c020000,

    Devices1_phys_base   = 0x1c100000,
    Timer0_phys_base      = Devices1_phys_base + 0x00010000,

    Devices2_phys_base   = 0x2c000000,
    Mp_scu_phys_base      = Devices2_phys_base,

    L2cxx0_phys_base      = Devices2_phys_base + 0x00003000,
  };
#endif
};
