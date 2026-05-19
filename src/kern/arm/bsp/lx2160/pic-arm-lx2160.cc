
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    0x06000000,  // distributor
    0x06200000, 0x200000); // redist

