
#include <platform_iface.h>
#include <static_init.h>
#include <mem_layout.h>
#include <globalconfig.h>

namespace {

struct Bsp_pf : Platform_if_base
{
  Address scu_phys() override
  {
#ifdef CONFIG_PF_IMX_6
    return Mem_layout::Mp_scu_phys_base;
#else
    return 0;
#endif
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Bsp_pf __pf;

}

