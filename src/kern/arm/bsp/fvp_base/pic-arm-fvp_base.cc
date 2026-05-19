
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    0x2f000000,  // distributor
    0x2f100000, 0x00200000,  // redist
    0x2f020000, 0x00020000); // its

