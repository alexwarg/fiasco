
#include <pic-gic-helper.h>

#include <irq_mgr_multi_chip.h>
#include <mmio_register_block.h>
#include <kmem.h>
#include <gic_v2.h>
#include <gic_v3.h>
#include <tramp-mp.h>
#include <globalconfig.h>

#include <cstdio>

namespace Pic_gic
{
#ifdef CONFIG_HAVE_ARM_GICV2
  static int
  add_gicv2(Address dist_virt, Gic_info const &inf,
            Irq_mgr_dyn *mgr,
            unsigned base = 0, bool primary = true)
  {
    printf("GICv2\n");
    Address cpu_v = Kmem::mmio_remap(inf.cpu_phys, inf.cpu_size);
    Gic_v2 *g = new Boot_object<Gic_v2>(cpu_v, dist_virt);
    if (primary)
      g->set_as_primary_irq_handler();

    if (int res = mgr->add_chip(base, g, g->nr_irqs()))
      return res;

    if (primary)
      tramp_mp_setup_gic_info(&inf, 2);

    return 0;
  }
#endif

#ifdef CONFIG_HAVE_ARM_GICV3
  static int
  add_gicv3(Address dist_virt, Gic_info const &inf,
            Irq_mgr_dyn *mgr,
            unsigned base = 0, bool primary = true)
  {
    printf("GICv3\n");
    Address redist_v = Kmem::mmio_remap(inf.redist_phys, inf.redist_size);
    Gic_v3 *g = new Boot_object<Gic_v3>(dist_virt, redist_v);
    if (primary)
      g->set_as_primary_irq_handler();

    if (int res = mgr->add_chip(base, g, g->nr_irqs()))
      return res;

    if (primary)
      tramp_mp_setup_gic_info(&inf, 3);

    return 0;
  }
#endif

  int add_gic(Irq_mgr_dyn *mgr, Gic_info const &inf)
  {
    if (inf.primary)
      tramp_mp_setup_gic_info(nullptr, 0);

    if (inf.dist_size < 0x1000)
      {
        printf("error: invalid GIC distributor mmio size (%lx)\n",
               inf.dist_size);
        return -L4_err::EInval;
      }

    Mmio_register_block dist(Kmem::mmio_remap(inf.dist_phys, inf.dist_size));
    unsigned vers = inf.version;
    if (vers == 0)
      {
        // detect GIC version
        if ((dist.read<Unsigned32>(0xfe8) & 0x0f0) == 0x20)
          vers = 2;
        else if (inf.dist_size < 0x10000)
          vers = 0;
        else if ((dist.read<Unsigned32>(0xffe8) & 0x0f0) == 0x30)
          vers = 3;
      }

    switch (vers)
      {
      case 2:
#ifdef CONFIG_HAVE_ARM_GICV2
        return add_gicv2(dist.get_mmio_base(), inf, mgr, inf.offset, inf.primary);
#else
        return -L4_err::ENodev;
#endif
      case 3:
#ifdef CONFIG_HAVE_ARM_GICV3
        return add_gicv3(dist.get_mmio_base(), inf, mgr, inf.offset, inf.primary);
#else
        return -L4_err::ENodev;
#endif
      default:
        return -L4_err::EInval;
      }
  }

  int add_gic(Gic_info const &inf)
  {
    typedef Irq_mgr_multi_chip<9> Mgr;
    Mgr *m = new Boot_object<Mgr>(8);
    Irq_mgr::mgr = m;
    return add_gic(m, inf);
  }
}

