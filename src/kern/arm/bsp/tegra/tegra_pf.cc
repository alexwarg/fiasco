
#include <platform_iface.h>
#include <static_init.h>
#include <mem_layout.h>

namespace {

struct Bsp_pf : Platform_if_base
{
  Address scu_phys() override
  {
    return Mem_layout::Mp_scu_phys_base;
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Bsp_pf __pf;

}

