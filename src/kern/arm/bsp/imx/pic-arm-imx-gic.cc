#include <globalconfig.h>
#include <pic-gic-helper.h>
#include <mem_layout.h>

#ifdef CONFIG_HAVE_ARM_GICV2
Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
    Mem_layout::Gic_dist_phys_base,  // distributor
    Mem_layout::Gic_cpu_phys_base); // cpu iface
#endif

#ifdef CONFIG_HAVE_ARM_GICV3
Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    Mem_layout::Gic_dist_phys_base,  // distributor
    Mem_layout::Gic_redist_phys_base, Mem_layout::Gic_redist_size); // redist
#endif
