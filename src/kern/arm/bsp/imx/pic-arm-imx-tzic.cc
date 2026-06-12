#include "boot_alloc.h"
#include "gic_tzic.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"
#include "mem_layout.h"

namespace Arm_imx_tzic {

Irq_mgr *
create_irq_mgr(bool)
{
  void *base = Kmem_mmio::map(Mem_layout::Gic_dist_phys_base, 0x1000);
  auto *g = new Boot_object<Gic_tzic>(base);
  auto *m = new Boot_object<Irq_mgr_single_chip<Gic_tzic>>(*g);
  Irq_mgr::mgr = m;
  g->set_as_primary_irq_handler();
  return m;
}

} // namespace Arm_imx_tzic
