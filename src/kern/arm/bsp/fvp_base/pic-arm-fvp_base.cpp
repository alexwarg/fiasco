INTERFACE [arm && pic_gic && pf_fvp_base]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base]:

#include "boot_alloc.h"
//#include "irq_mgr_msi.h"
#include "irq_mgr.h"
#include "gic_v3.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  using Mgr = Irq_mgr_single_chip<Gic_v3>;
  Mgr *m = new Boot_object<Mgr>(Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                                 Gic_dist::Size),
                                Kmem::mmio_remap(Mem_layout::Gic_redist_phys_base,
                                                 Mem_layout::Gic_redist_size));
#if 0
  if (Gic_v3::Have_lpis)
    m->c.add_its(Kmem::mmio_remap(Mem_layout::Gic_its_phys_base,
                                  Mem_layout::Gic_its_size));
#endif
  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_fvp_base]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
