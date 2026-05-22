#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
#ifdef CONFIG_PF_IMX
  enum Phys_layout_imx : Address {
    Flush_area_phys_base = 0xe0000000,
  };
#endif

#ifdef CONFIG_PF_IMX_21
  enum Phys_layout_imx21 : Address {
    Timer_phys_base       = 0x10003000,
    Pll_phys_base         = 0x10027000,
    Watchdog_phys_base    = 0x10002000,
    Pic_phys_base         = 0x10040000,
  };
#endif

#ifdef CONFIG_PF_IMX_28
  enum Phys_layout_imx28 : Address {
    Timer_phys_base       = 0x80068000,
    Watchdog_phys_base    = 0x80056000,
    Pic_phys_base         = 0x80000000,
  };
#endif

#ifdef CONFIG_PF_IMX_35
  enum Phys_layout_imx35 : Address {
    Timer_phys_base       = 0x53f94000,
    Watchdog_phys_base    = 0x53fdc000,
    Pic_phys_base         = 0x68000000,
  };
#endif

#ifdef CONFIG_PF_IMX_51
  enum Phys_layout_imx51 : Address {
    Timer_phys_base       = 0x73fac000,
    Watchdog_phys_base    = 0x73f98000,
    Gic_dist_phys_base    = 0xe0000000,
    Gic_cpu_phys_base     = 0xe0000000,
  };
#endif

#ifdef CONFIG_PF_IMX_53
  enum Phys_layout_imx53 : Address {
    Timer_phys_base       = 0x53fac000,
    Watchdog_phys_base    = 0x53f98000,
    Gic_dist_phys_base    = 0x0fffc000,
    Gic_cpu_phys_base     = 0x0fffc000,
  };
#endif

#ifdef CONFIG_PF_IMX_6
  enum Phys_layout_imx6 : Address {
    Timer_phys_base      = 0x020d0000,
    Mp_scu_phys_base     = 0x00a00000,
    Gic_cpu_phys_base    = 0x00a00100,
    Gic_dist_phys_base   = 0x00a01000,
    L2cxx0_phys_base     = 0x00a02000,

    Watchdog_phys_base   = 0x020bc000,
    Gpt_phys_base        = 0x02098000,
    Src_phys_base        = 0x020d8000,
  };
#endif

#ifdef CONFIG_PF_IMX_6UL
  enum Phys_layout_imx6ul : Address {
    Gic_dist_phys_base   = 0x00a01000,
    Gic_cpu_phys_base    = 0x00a02000,
    Gic_h_phys_base      = 0x00a04000,
    Gic_v_phys_base      = 0x00a06000,

    Watchdog_phys_base   = 0x020bc000,
  };
#endif

#ifdef CONFIG_PF_IMX_7
  enum Phys_layout_imx7 : Address {
    Gic_dist_phys_base   = 0x31001000,
    Gic_cpu_phys_base    = 0x31002000,
    Gic_h_phys_base      = 0x31004000,
    Gic_v_phys_base      = 0x31006000,

    Watchdog_phys_base   = 0x30280000,
    Src_phys_base        = 0x30390000,
    Gpc_phys_base        = 0x303a0000,
  };
#endif

#ifdef CONFIG_PF_IMX_8M
  enum Phys_layout_imx8m : Address {
    /*dummy*/ Watchdog_phys_base   = ~0UL
  };
#endif

#ifdef CONFIG_PF_IMX_8XQ
  enum Phys_layout_imx8xq : Address {
    Gic_dist_phys_base   = 0x51a00000,
    Gic_redist_phys_base = 0x51b00000,
    Gic_redist_size      = 0x00100000,
    /*dummy*/ Watchdog_phys_base   = ~0UL
  };
#endif
};
