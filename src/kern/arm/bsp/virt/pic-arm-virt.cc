
#include <pic-gic-helper.h>
#include <mem_layout.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info = Pic_gic::gic_vx_info(
    0x08000000,   // dist
    0x08010000,   // v2 cpu if
    0x080A0000, 0x00F60000); // redist

