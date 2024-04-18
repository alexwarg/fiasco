#pragma once

#include <globalconfig.h>
#include <mem_layout.h>

#ifdef CONFIG_PF_OMAP3_AM33XX

#include <timer_omap3.h>
#include <timer_omap3_am33xx.h>

// use the 1ms timer
struct Timer_1ms : Timer_omap3
{
  static unsigned irq()
  { return 67; }

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      init_am33xx(Mem_layout::Cm_wkup_phys_base, Mem_layout::Cm_dpll_phys_base);
  }
};

struct Timer_timer0 : Timer_omap3_am33xx
{
  static unsigned irq()
  { return 66; }

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      init_timer(Mem_layout::Cm_wkup_phys_base);
  }
};

using Timer = Timer_1ms;
#endif // CONFIG_PF_OMAP3_AM33XX

#if defined (CONFIG_PF_OMAP3_BEAGLEBOARD) || defined (CONFIG_PF_OMAP3_OMAP35XEVM)

#include <timer_omap3.h>

struct Timer : Timer_omap3
{
  static unsigned irq() { return 37; }

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      init_35xx(Mem_layout::Wkup_cm_phys_base + 0x40);
  }
};
#endif // CONFIG_PF_OMAP3_BEAGLEBOARD || CONFIG_PF_OMAP3_OMAP35XEVM

#ifdef CONFIG_PF_OMAP4_PANDABOARD

#include <timer_arm_mptimer.h>

using Timer = Timer_arm_mptimer_t<499999>;

#endif // CONFIG_PF_OMAP4_PANDABOARD

#ifdef CONFIG_PF_OMAP5_5432EVM

#include <timer_arm_generic.h>

struct Timer : Timer_generic_timer
{
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 30; // we use this mode in TZ secure mode (so sec IRQ)
      case Generic_timer::Virtual:  return 27;
      case Generic_timer::Hyp:      return 26;
      };
  }

  static void init(Cpu_number cpu)
  {
    if (!check_and_disable(cpu))
      return;

    if (is_boot_cpu(cpu))
      set_freq0(6144000);

    finalize_init(cpu);
  }

};

#endif // CONFIG_PF_OMAP5_5432EVM

