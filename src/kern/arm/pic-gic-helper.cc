
#include <pic-gic-helper.h>

#include <mmio_register_block.h>
#include <kmem_mmio.h>
#include <gic_v2.h>
#include <gic_v3.h>
#include <vgic_v2.h>
#include <vgic_v3.h>
#include <vgic_global.h>
#include <tramp-mp.h>
#include <globalconfig.h>

#ifdef CONFIG_ARM_GIC_MSI
#include <gic_msi.h>
#include <irq_mgr_multi_msi.h>
using Irq_mgr_arm = Irq_mgr_multi_msi;
#else
#include <irq_mgr_multi_chip.h>
using Irq_mgr_arm = Irq_mgr_multi_chip<9>;
#endif

#include <cstdio>

namespace Pic_gic
{
#ifdef CONFIG_HAVE_ARM_GICV2
  static int
  add_gicv2(void *dist_virt, Gic_info const &inf,
            Irq_mgr_dyn *mgr,
            unsigned base = 0, bool primary = true)
  {
    printf("GICv2\n");
    void *cpu_v = Kmem_mmio::map(inf.cpu_phys, inf.cpu_size);
    Gic_v2 *g = new Boot_object<Gic_v2>(cpu_v, dist_virt);
    if (primary)
      g->set_as_primary_irq_handler();

    if (int res = mgr->add_chip(base, g, g->nr_irqs()))
      return res;

    if (primary)
      tramp_mp_setup_gic_info(&inf, 2);

    if (IS_ENABLED(CONFIG_CPU_VIRT) && primary)
      {
        if (!inf.cpu_h_size || !inf.cpu_v_size)
          {
            // no vGIC need to disable VM support (FIXME)
            return -L4_err::ENodev;
          }

        Gic_h_global::gic =
          new Boot_object<Gic_h_v2>(Kmem_mmio::map(inf.cpu_h_phys, inf.cpu_h_size),
                                    inf.cpu_v_phys);
      }

    return 0;
  }

  int create_gicv2(Irq_mgr_dyn *mgr, Gic_info const &inf)
  {
    if (inf.primary)
      tramp_mp_setup_gic_info(nullptr, 0);

    if (inf.dist_size < 0x1000)
      {
        printf("error: invalid GIC distributor mmio size (%lx)\n",
               inf.dist_size);
        return -L4_err::EInval;
      }

    Mmio_register_block dist(Kmem_mmio::map(inf.dist_phys, inf.dist_size));
    return add_gicv2((void *)dist.get_mmio_base(), inf, mgr, inf.offset, inf.primary);
 }
#endif

#ifdef CONFIG_HAVE_ARM_GICV3
  static int
  add_gicv3(void *dist_virt, Gic_info const &inf,
            Irq_mgr_dyn *mgr,
            unsigned base = 0, bool primary = true)
  {
    printf("GICv3\n");
    void *redist_v = Kmem_mmio::map(inf.redist_phys, inf.redist_size);
    Gic_v3 *g = new Boot_object<Gic_v3>(dist_virt, redist_v);
    if (primary)
      g->set_as_primary_irq_handler();

    if (int res = mgr->add_chip(base, g, g->nr_irqs()))
      return res;

    if (primary)
      tramp_mp_setup_gic_info(&inf, 3);

    if (IS_ENABLED(CONFIG_CPU_VIRT) && primary)
      Gic_h_global::gic = new Boot_object<Gic_h_v3>();

    if (inf.its_phys && inf.its_size)
      {
#ifdef CONFIG_ARM_GIC_MSI
        if (primary)
          mgr->add_msi_chip(g->msi_chip());
#endif
        g->add_its(Kmem_mmio::map(inf.its_phys, inf.its_size));
      }

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

    Mmio_register_block dist(Kmem_mmio::map(inf.dist_phys, inf.dist_size));
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
        return add_gicv2((void *)dist.get_mmio_base(), inf, mgr, inf.offset, inf.primary);
#else
        return -L4_err::ENodev;
#endif
      case 3:
#ifdef CONFIG_HAVE_ARM_GICV3
        return add_gicv3((void *)dist.get_mmio_base(), inf, mgr, inf.offset, inf.primary);
#else
        return -L4_err::ENodev;
#endif
      default:
        return -L4_err::EInval;
      }
  }

  int add_gic(Gic_info const &inf)
  {
    typedef Irq_mgr_arm Mgr;
    Mgr *m = new Boot_object<Mgr>(8);
    Irq_mgr::mgr = m;
    return add_gic(m, inf);
  }
}

