
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v2_info(
    0xf9010000,  // distributor
    0xf9020000); // cpu iface

