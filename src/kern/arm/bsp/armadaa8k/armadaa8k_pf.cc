
#include <pic-gic-helper.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct Default_pf : Platform_base
{
  void init_irqs() override
  {
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Default_pf __pf;

}
