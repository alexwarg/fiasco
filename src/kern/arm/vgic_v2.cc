
#include "vgic_v2.h"

#include "vgic_global.h"
#include "boot_alloc.h"
#include "pic.h"

Static_object<Gic_h_v2> Gic_h_v2::gic;


namespace {

struct Gic_h_v2_init
{
  explicit Gic_h_v2_init()
  {
    if (Pic::gic->gic_version() > 2)
      return;

    Gic_h_global::gic
      = new Boot_object<Gic_h_v2>(Kmem::mmio_remap(Mem_layout::Gic_h_phys_base,
                                                   Config::PAGE_SIZE));
  }
};

Gic_h_v2_init __gic_h;
}

