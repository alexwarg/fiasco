
#include <pic-gic-helper.h>
#include <mem_layout.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
    Mem_layout::Gic_dist_phys_base,  // distributor
    Mem_layout::Gic_cpu_phys_base); // cpu iface

