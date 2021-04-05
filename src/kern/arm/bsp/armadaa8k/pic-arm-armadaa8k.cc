
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info =
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys   = 0xf0210000, .dist_size   = 0x1000,
  .cpu_phys    = 0xf0220000, .cpu_size    = 0x100,
  .cpu_h_phys  = 0xf0240000, .cpu_h_size  = 0x2000,
  .cpu_v_phys  = 0xf0260000, .cpu_v_size  = 0x2000,
};
