
#include <pic-gic-dt.h>
#include <device_tree.h>
#include <dt-arm.h>
#include <kmem_mmio.h>
#include <boot_alloc.h>
#include <tramp-mp.h>
#include <pic-gic-helper.h>
#include <globalconfig.h>
#include <gic_v2.h>
#include <gic_v3.h>
#include <gic_redist.h>
#include <vgic_v2.h>
#include <vgic_v3.h>
#include <vgic_global.h>
#include <l4_types.h>
#include <irq_mgr.h>

#ifdef CONFIG_ARM_GIC_MSI
#include <gic_msi.h>
#include <irq_mgr_multi_msi.h>
using Irq_mgr_arm = Irq_mgr_multi_msi;
#else
#include <irq_mgr_multi_chip.h>
using Irq_mgr_arm = Irq_mgr_multi_chip<9>;
#endif

namespace {

#ifdef CONFIG_HAVE_ARM_GICV3

class Gic_redist_find_dt : public Gic_redist_find
{
  Device_tree::Node _node;
  unsigned _num_redists;

public:
  Gic_redist_find_dt(Device_tree::Node node, unsigned num_redists)
  : _node(node), _num_redists(num_redists)
  {}

  Mmio_register_block get_redist_mmio(Unsigned64 mpid) override
  {
    for (unsigned i = 1; i <= _num_redists; i++)
      {
        uint64_t base, size;
        if (!_node.get_reg(i, &base, &size))
          break;

        void *va = Kmem_mmio::map(base, size);
        Mmio_register_block r = scan_range(reinterpret_cast<Address>(va), mpid);
        if (r.get_mmio_base())
          return r;
      }
    return Mmio_register_block(nullptr);
  }
};

static int
init_gicv3(Device_tree::Node node)
{
  uint64_t dist_phys, dist_size;
  if (!node.get_reg(0, &dist_phys, &dist_size))
    return -L4_err::EInval;

  uint32_t num_redists = node.get_prop_default_u32("#redistributor-regions", 1);

  auto *redist_find = new Boot_object<Gic_redist_find_dt>(node, num_redists);

  void *dist_v = Kmem_mmio::map(dist_phys, dist_size);
  auto *g = new Boot_object<Gic_v3>(dist_v, redist_find);
  g->set_as_primary_irq_handler();

  auto *mgr = new Boot_object<Irq_mgr_arm>(8);
  Irq_mgr::mgr = mgr;
  if (int res = mgr->add_chip(0, g, g->nr_irqs()))
    return res;

  Pic_gic::Gic_info inf{};
  inf.version   = 3;
  inf.primary   = true;
  inf.dist_phys = dist_phys;
  inf.dist_size = dist_size;
  tramp_mp_setup_gic_info(&inf, 3);

  if (IS_ENABLED(CONFIG_CPU_VIRT))
    Gic_h_global::gic = new Boot_object<Gic_h_v3>();

  Device_tree::dt.nodes_by_compatible("arm,gic-v3-its",
    [&](Device_tree::Node its_node)
    {
      if (!its_node.is_enabled())
        return Device_tree::Continue;

      uint64_t its_phys, its_size;
      if (!its_node.get_reg(0, &its_phys, &its_size))
        return Device_tree::Continue;

      void *its_v = Kmem_mmio::map(its_phys, its_size);
      g->add_its(its_v);

#ifdef CONFIG_ARM_GIC_MSI
      mgr->add_msi_chip(g->msi_chip());
#endif
      return Device_tree::Continue;
    });

  return 0;
}

#endif // CONFIG_HAVE_ARM_GICV3


#ifdef CONFIG_HAVE_ARM_GICV2

static int
init_gicv2(Device_tree::Node node)
{
  uint64_t dist_phys, dist_size;
  if (!node.get_reg(0, &dist_phys, &dist_size))
    return -L4_err::EInval;

  uint64_t cpu_phys, cpu_size;
  if (!node.get_reg(1, &cpu_phys, &cpu_size))
    return -L4_err::EInval;

  void *dist_v = Kmem_mmio::map(dist_phys, dist_size);
  void *cpu_v  = Kmem_mmio::map(cpu_phys, cpu_size);

  auto *g = new Boot_object<Gic_v2>(cpu_v, dist_v);
  g->set_as_primary_irq_handler();

  auto *mgr = new Boot_object<Irq_mgr_arm>(8);
  Irq_mgr::mgr = mgr;
  if (int res = mgr->add_chip(0, g, g->nr_irqs()))
    return res;

  Pic_gic::Gic_info inf{};
  inf.version   = 2;
  inf.primary   = true;
  inf.dist_phys = dist_phys;
  inf.dist_size = dist_size;
  inf.cpu_phys  = cpu_phys;
  inf.cpu_size  = cpu_size;
  tramp_mp_setup_gic_info(&inf, 2);

  if (IS_ENABLED(CONFIG_CPU_VIRT))
    {
      uint64_t h_phys, h_size;
      uint64_t v_phys, v_size;
      if (node.get_reg(2, &h_phys, &h_size) && node.get_reg(3, &v_phys, &v_size))
        Gic_h_global::gic =
          new Boot_object<Gic_h_v2>(Kmem_mmio::map(h_phys, h_size), v_phys);
    }

  return 0;
}

#endif // CONFIG_HAVE_ARM_GICV2

} // anonymous namespace


int
Pic_gic_dt::init()
{
  tramp_mp_setup_gic_info(nullptr, 0);

#ifdef CONFIG_HAVE_ARM_GICV3
  {
    Device_tree::Node n = Device_tree::dt.node_by_compatible("arm,gic-v3");
    if (n.is_valid() && n.is_enabled())
      return init_gicv3(n);
  }
#endif

#ifdef CONFIG_HAVE_ARM_GICV2
  {
    static char const * const compat[] = {
      "arm,cortex-a15-gic",
      "arm,gic-400",
      "arm,cortex-a9-gic",
      "arm,cortex-a7-gic",
      "arm,cortex-a5-gic",
    };
    Device_tree::Node n = Device_tree::dt.node_by_compatible_list(compat);
    if (n.is_valid() && n.is_enabled())
      return init_gicv2(n);
  }
#endif

  return -L4_err::ENodev;
}
