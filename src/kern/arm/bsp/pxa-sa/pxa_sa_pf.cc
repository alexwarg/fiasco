
#include <irqs_pxa_sa.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct Pxa_sa_pf : Platform_base
{
  void init_irqs() override
  {
    Arm_pxa_sa::create_irq_mgr(true);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Pxa_sa_pf __pf;

}
