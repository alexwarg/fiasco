
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_v3_info(
    0xd1d00000,  // distributor
    0xd1d40000, 0x00040000); // redist

