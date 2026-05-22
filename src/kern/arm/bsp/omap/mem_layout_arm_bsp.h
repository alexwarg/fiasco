#pragma once

#include <globalconfig.h>
#include "types.h"

class Mem_layout_arm_bsp
{
public:
#ifdef CONFIG_PF_OMAP3_35X
  enum Phys_layout_omap3_35x : Address {
    Wkup_cm_phys_base        = 0x48004c00,
    L4_addr_prot_phys_base   = 0x48040000,
    Gptimer10_phys_base      = 0x48086000,

    Intc_phys_base           = 0x48200000,

    Prm_global_reg_phys_base = 0x48307200,
    Timer1ms_phys_base       = 0x48318000,
  };
#endif

#ifdef CONFIG_PF_OMAP3_AM33XX
  enum Phys_layout_omap3_335x : Address {
    Cm_per_phys_base         = 0x44e00000,
    Cm_wkup_phys_base        = 0x44e00400,
    Cm_dpll_phys_base        = 0x44e00500,
    Timergen_phys_base       = 0x44e05000,
    Timer1ms_phys_base       = 0x44e31000,
    Prm_global_reg_phys_base = 0x48107200,
    Intc_phys_base           = 0x48200000,
  };
#endif

#ifdef CONFIG_PF_OMAP4
  enum Phys_layout_omap4_pandaboard : Address {
    Mp_scu_phys_base        = 0x48240000,
    L2cxx0_phys_base        = 0x48242000,

    __Timer                 = 0x48240600,

    Prm_phys_base           = 0x4a306000,
  };
#endif

#ifdef CONFIG_PF_OMAP5
  enum Phys_layout_omap5 : Address {
    Prm_phys_base           = 0x4ae06000,
  };
#endif
};
