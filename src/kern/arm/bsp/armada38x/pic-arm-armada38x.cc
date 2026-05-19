
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
    0xf100d000,  // distributor
    0xf100c100); // cpu iface
