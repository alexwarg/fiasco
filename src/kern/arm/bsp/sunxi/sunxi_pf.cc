
#include <pic-gic-helper.h>
#include <platform_generic.h>
#include <static_init.h>
#include <mem_layout.h>

namespace {

struct Bsp_pf : Platform_base
{
  Address scu_phys() override
  {
    return Mem_layout::Mp_scu_phys_base;
  }

  void init_irqs() override
  {
    Pic_gic::add_gic(Pic_gic::primary_gic_info);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Bsp_pf __pf;

}
