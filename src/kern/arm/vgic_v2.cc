
#include <vgic_v2.h>
#include <gic_iface.h>

#include <vgic_global.h>
#include <boot_alloc.h>
#include <kmem.h>

namespace {

struct Gic_h_v2_init
{
  explicit Gic_h_v2_init()
  {
    if (Gic::primary->gic_version() > 2)
      return;

    Gic_h_global::gic
      = new Boot_object<Gic_h_v2>(Kmem::mmio_remap(Mem_layout::Gic_h_phys_base,
                                                   Config::PAGE_SIZE),
                                  Mem_layout::Gic_v_phys_base);
  }
};

Gic_h_v2_init __gic_h;
}

