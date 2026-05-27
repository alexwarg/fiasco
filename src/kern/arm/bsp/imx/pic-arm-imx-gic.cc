#include <globalconfig.h>
#include <pic-gic-helper.h>
#include <mem_layout.h>

#ifdef CONFIG_HAVE_ARM_GICV2
#ifndef CONFIG_ARM_IMX_MXC_TZIC
Pic_gic::Gic_info const Pic_gic::primary_gic_info =
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys  = Mem_layout::Gic_dist_phys_base, .dist_size = 0x1000,
  .cpu_phys   = Mem_layout::Gic_cpu_phys_base,  .cpu_size = 0x100,
#if defined (CONFIG_PF_IMX_6UL) || defined (CONFIG_PF_IMX_7)
  .cpu_h_phys = Mem_layout::Gic_h_phys_base,    .cpu_h_size = 0x1000,
  .cpu_v_phys = Mem_layout::Gic_v_phys_base,    .cpu_v_size = 0x1000,
#endif
};
#endif
#endif

#ifdef CONFIG_HAVE_ARM_GICV3
Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    0x38800000,  // distributor
    0x38880000, 0x000c0000); // redist
#endif
