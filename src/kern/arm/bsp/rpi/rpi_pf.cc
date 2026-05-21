
#include <irqs_rpi.h>
#include <platform_generic.h>
#include <static_init.h>
#include <globalconfig.h>

namespace {

struct Rpi_pf : Platform_base
{
  void init_irqs() override
  {
#ifdef CONFIG_ARM_GIC
    Arm_rpi::create_irq_mgr_gic(true);
#elif defined(CONFIG_PF_RPI_RPI1) || defined(CONFIG_PF_RPI_RPIZW)
    Arm_rpi::create_irq_mgr_bcm(true);
#else
    Arm_rpi::create_irq_mgr_bcm2836(true);
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Rpi_pf __pf;

}
