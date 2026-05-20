
#include <pic-gic-helper.h>
#include <globalconfig.h>

Pic_gic::Gic_info const Pic_gic::primary_gic_info =
{
#ifdef CONFIG_PF_OMAP4
  .version = 2, .primary = true, .offset = 0,
  .dist_phys = 0x48241000, .dist_size = 0x1000,
  .cpu_phys  = 0x48240100, .cpu_size  = 0x100,
#endif
#ifdef CONFIG_PF_OMAP5
  .version = 2, .primary = true, .offset = 0,
  .dist_phys = 0x48211000, .dist_size = 0x1000,
  .cpu_phys  = 0x48212000, .cpu_size  = 0x100,

  .cpu_h_phys = 0x48214000, .cpu_h_size = 0x1000,
  .cpu_v_phys = 0x48216000, .cpu_v_size = 0x1000,
#endif
};
