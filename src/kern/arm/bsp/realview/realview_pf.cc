
#include <platform_generic.h>
#include <static_init.h>
#include <kmem.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <rv_platforms.h>
#include <cascade_irq.h>
#include <irq_mgr_multi_chip.h>
#include <pic-gic-helper.h>
#include <platform.h>

namespace {

struct Realview_pf : Platform_base
{
  Address scu_phys() override
  {
    return rv_current_platform()->scu;
  }

  void init_irqs()
  {
    Rv_pf const *pf = rv_current_platform();
    if (pf->syscon_gic)
      {
        enum { INTMODE_NEW_NO_DDC = 1 << 23 };
        Register_block<32> sys(Kmem::mmio_remap(pf->sys_r, 0x1000));
        sys[Platform::Sys::Lock] = 0xa05f;
        sys[Platform::Sys::Pld_ctrl1].modify(INTMODE_NEW_NO_DDC, 0x7ul << 22);
        sys[Platform::Sys::Lock] = 0x0000;
      }

    typedef Irq_mgr_multi_chip<8> Mgr;
    Mgr *m = new Boot_object<Mgr>(4);
    Irq_mgr::mgr = m;

    for (auto const &g: pf->g())
      {
        if (Pic_gic::add_gic(m, g) < 0)
          panic("error setting up IRQ controller");

        if (g.parent_irq >= 0)
          {
            Irq_chip_icu *c = m->chip(g.offset).chip;
            Irq_base *casc_irq = new Boot_object<Cascade_irq>(c, c->get_cascade_hit());
            m->chip(g.parent_irq).chip->alloc(casc_irq, g.parent_irq);
            casc_irq->unmask();
          }
      }
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Realview_pf __pf;

}

