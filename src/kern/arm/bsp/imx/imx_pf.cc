
#include <irqs_imx.h>
#include <irq_chip_arm_tzic.h>
#include <pic-gic-helper.h>
#include <platform_generic.h>
#include <static_init.h>
#include <mem_layout.h>
#include <globalconfig.h>

namespace {

struct Bsp_pf : Platform_base
{
  Address scu_phys() override
  {
#ifdef CONFIG_PF_IMX_6
    return Mem_layout::Mp_scu_phys_base;
#else
    return 0;
#endif
  }

  void init_irqs() override
  {
#if defined(CONFIG_PF_IMX_51) || defined(CONFIG_PF_IMX_53)
    Arm_imx_tzic::create_irq_mgr(true);
#elif defined(CONFIG_PF_IMX_6) || defined(CONFIG_PF_IMX_6UL) \
      || defined(CONFIG_PF_IMX_7) || defined(CONFIG_ARM_V8)
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
#elif defined(CONFIG_PF_IMX_28)
    Arm_imx_icoll::create_irq_mgr(true);
#else
    Arm_imx::create_irq_mgr(true);
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Bsp_pf __pf;

}
