
#include <platform_dt.h>
#include <pic-gic-helper.h>
#include <static_init.h>

namespace {

struct Default_pf : Platform_dt
{
  void init_irqs() override
  {
    if (init_irqs_dt() == 0)
      return;
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Default_pf __pf;

}
