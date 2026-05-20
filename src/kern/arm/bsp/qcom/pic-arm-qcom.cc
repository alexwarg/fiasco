
#include <pic-gic-helper.h>
#include <mem_layout.h>
#include <globalconfig.h>

#ifdef CONFIG_HAVE_ARM_GICV2
Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys  = Mem_layout::Gic_dist_phys_base, .dist_size = 0x1000,
  .cpu_pyhs   = Mem_layout::Gic_cpu_phys_base,  .cpu_size = 0x100,
  .cpu_h_phys = Mem_layout::Gic_h_phys_base,    .cpu_h_size = 0x1000,
  .cpu_v_phys = Mem_layout::Gic_v_phys_base,    .cpu_v_size = 0x1000,
};
#endif

#ifdef CONFIG_HAVE_ARM_GICV3
Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    Mem_layout::Gic_dist_phys_base,
    Mem_layout::Gic_redist_phys_base, Mem_layout::Gic_redist_size,
    Mem_layout::Gic_its_phys_base, Mem_layout::Gic_its_size);
#endif
