#pragma once

#include <globalconfig.h>

#ifdef CONFIG_ARM_GENERIC_TIMER

#include <timer_arm_generic.h>

struct Timer : Timer_generic_timer
{
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical:
      case Generic_timer::Virtual: return 27;
      case Generic_timer::Hyp:     return 26;
      };
  }
};

#endif

#ifdef CONFIG_ARM_MPTIMER

#include <timer_arm_imx6_mptimer.h>
using Timer = Timer_arm_imx6_mptimer;

#endif

#ifdef CONFIG_ARM_IMX_TIMER_EPIT
#include <timer_arm_imx_epit.h>
#include <timer_arm_imx_wrapper.h>
#include <mem_layout.h>

#ifdef CONFIG_PF_IMX_35
using Timer = Timer_arm_imx_wrapper<Timer_arm_imx_epit, 28, Mem_layout::Timer_phys_base>;
#endif
#if defined (CONFIG_PF_IMX_51) || defined (CONFIG_IMX_53)
using Timer = Timer_arm_imx_wrapper<Timer_arm_imx_epit, 40, Mem_layout::Timer_phys_base>;
#endif
#ifdef CONFIG_PF_IMX_6
using Timer = Timer_arm_imx_wrapper<Timer_arm_imx_epit, 88, Mem_layout::Timer_phys_base>;
#endif

#endif

#ifdef CONFIG_PF_IMX_21
#include <timer_arm_imx21.h>
#include <timer_arm_imx_wrapper.h>
#include <mem_layout.h>

using Timer = Timer_arm_imx_wrapper<Timer_arm_imx21, 26, Mem_layout::Timer_phys_base, 0x100>;
#endif

#ifdef CONFIG_PF_IMX_28

#include <timer_arm_imx_timrot.h>
#include <timer_arm_imx_wrapper.h>
#include <mem_layout.h>

using Timer = Timer_arm_imx_wrapper<Timer_imx_timrot, 48, Mem_layout::Timer_phys_base, 0x100>;
#endif
