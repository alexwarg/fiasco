
#include <irqs_kirkwood.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct Kirkwood_pf : Platform_base
{
  void init_irqs() override
  {
    Arm_kirkwood::create_irq_mgr(true);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Kirkwood_pf __pf;

}
