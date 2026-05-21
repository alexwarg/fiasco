
#include <irqs_omap3.h>
#include <pic-gic-helper.h>
#include <platform_generic.h>
#include <static_init.h>
#include <globalconfig.h>

namespace {

struct Omap_pf : Platform_base
{
  Address scu_phys() override
  {
    return 0x48240000;
  }

  void init_irqs() override
  {
#if defined(CONFIG_ARM_GIC)
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
#else
    Arm_omap3::create_irq_mgr(true);
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Omap_pf __pf;

}
