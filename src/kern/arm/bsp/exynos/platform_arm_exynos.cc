#include "platform_arm_exynos.h"

#include "io.h"
#include "config.h"
#include "kmem.h"
#include "mem_layout.h"

#ifdef CONFIG_PF_EXYNOS_EXTGIC
#include <feature.h>
KIP_KERNEL_FEATURE("exy:extgic");
#endif

Platform::Soc_type Platform::_soc;
unsigned Platform::_subrev;
unsigned Platform::_uart;

#ifdef CONFIG_PF_EXYNOS_USE_PKG_IDS
#include CONFIG_PF_EXYNOS_PKG_IDS
#endif

unsigned
Platform::subrev()
{
  return _subrev;
}

void
Platform::process_pkg_ids()
{
#ifdef CONFIG_PF_EXYNOS_USE_PKG_IDS
  if (sizeof(CONFIG_PF_EXYNOS_PKG_IDS) <= 1)
    return;
  Mword pkg_id = Io::read<Mword>(Kmem::mmio_remap(0x10000000 + 4, sizeof(Mword)));
  for (unsigned i = 0; i < sizeof(__pkg_ids) / sizeof(__pkg_ids[0]); ++i)
    if ((pkg_id & __pkg_ids[i].mask) == __pkg_ids[i].val)
      {
        _soc = (Soc_type)__pkg_ids[i].soc;
        _uart = __pkg_ids[i].uart;
        return;
      }
#endif
}

void
Platform::type()
{
  if (_soc == 0)
    {
      Mword pro_id = Io::read<Mword>(Kmem::mmio_remap(Mem_layout::Chip_id_phys_base,
                                                      sizeof(Mword)));

      _subrev = pro_id & 0xff;

      // set defaults from config
      _uart = Config::Uart_nr;
#if defined(CONFIG_PF_EXYNOS4_4210)
      _soc = Soc_4210;
#elif defined(CONFIG_PF_EXYNOS4_4412)
      _soc = Soc_4412;
#elif defined(CONFIG_PF_EXYNOS5_5250)
      _soc = Soc_5250;
#endif

      process_pkg_ids();
    }
}

bool
Platform::is_4210()
{ type(); return _soc == Soc_4210; }

bool
Platform::is_4412()
{ type(); return _soc == Soc_4412; }

bool
Platform::is_5250()
{ type(); return _soc == Soc_5250; }

bool
Platform::is_5410()
{ type(); return _soc == Soc_5410; }

unsigned
Platform::uart_nr()
{ type(); return _uart; }
