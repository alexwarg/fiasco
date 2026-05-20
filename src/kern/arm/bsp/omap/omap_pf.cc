
#include <platform_iface.h>
#include <static_init.h>

namespace {

struct Omap_pf : Platform_if_base
{
  Address scu_phys() override
  {
    return 0x48240000;
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Omap_pf __pf;

}

