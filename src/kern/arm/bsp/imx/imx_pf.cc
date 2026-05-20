
#include <pic.h>
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
#if defined(CONFIG_PF_IMX_51) || defined(CONFIG_PF_IMX_53) \
    || defined(CONFIG_PF_IMX_6) || defined(CONFIG_PF_IMX_6UL) \
    || defined(CONFIG_PF_IMX_7) || defined(CONFIG_ARM_V8)
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
#else
    Pic::init();
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Bsp_pf __pf;

}
