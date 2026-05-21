
#include <irqs_integrator.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct Integrator_pf : Platform_base
{
  void init_irqs() override
  {
    Arm_integrator::create_irq_mgr(true);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Integrator_pf __pf;

}
