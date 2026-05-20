
#include <platform_iface.h>
#include <static_init.h>
#include <cpu.h>
#include <globalconfig.h>

namespace {

struct Exynos_pf : Platform_if_base
{
  Address scu_phys() override
  {
    return 0x10000000;
  }

  void init() override
  {
#if defined (CONFIG_ARM_MPCORE) || defined (CONFIG_ARM_CORTEX_A9) || defined (CONFIG_ARM_CORTEX_A5)
    if (IS_ENABLED(CONFIG_ARM_EM_NS) && Cpu::scu.available())
      Cpu::scu.r[Scu::R::Control].set(Scu::Control::Scu_standby);
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Exynos_pf __pf;

}

