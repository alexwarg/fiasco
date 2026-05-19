
#include <vgic_v3.h>
#include <gic_iface.h>

#include <boot_alloc.h>
#include <vgic_global.h>

namespace {

struct Gic_h_v3_init
{
  explicit Gic_h_v3_init()
  {
    unsigned v = Gic::primary->gic_version();
    if (v < 3 || v > 4)
      return;

    Gic_h_global::gic = new Boot_object<Gic_h_v3>();
  }
};

Gic_h_v3_init __gic_h;
}

