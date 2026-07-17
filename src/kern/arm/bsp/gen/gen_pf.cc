
#include <platform_dt.h>
#include <static_init.h>
#include <panic.h>

namespace {

struct Gen_pf : Platform_dt
{
  void init_irqs() override
  {
    if (init_irqs_dt() != 0)
      panic("DT platform: no supported GIC found in device tree\n");
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Gen_pf __pf;

}
