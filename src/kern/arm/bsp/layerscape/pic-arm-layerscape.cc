
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
    0x01401000,  // distributor
    0x01402000); // cpu iface

