
#include <pic-gic-helper.h>
#include <pic-gic-dt.h>
#include <device_tree.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct Default_pf : Platform_base
{
  void init_irqs() override
  {
    if (Device_tree::dt.valid() && Pic_gic_dt::init() == 0)
      return;
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Default_pf __pf;

}
