
#include <pic-gic-helper.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info =
{
  .version = 0, .primary = true, .offset = 0,
  .dist_phys   = 0x08000000, .dist_size   = 0x10000,
  .cpu_phys    = 0x08010000, .cpu_size    = 0x1000,
  // virtual MMIO GICv2
  .cpu_h_phys  = 0x08030000, .cpu_h_size  = 0x1000,
  .cpu_v_phys  = 0x08040000, .cpu_v_size  = 0x1000,

  // GICv3 redistributor, if we have a GICv3
  .redist_phys = 0x080A0000, .redist_size = 0x00F60000,

  // GICv3 its, ...
  .its_phys    =  0x08080000, .its_size   = 0x00020000,
};
