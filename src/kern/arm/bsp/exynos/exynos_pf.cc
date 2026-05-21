
#include <platform_iface.h>
#include <static_init.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <irq_combiner.h>
#include <pic-gic-helper.h>
#include <exynos_irq_mgr.h>
#include "platform_arm_exynos.h"
#include <globalconfig.h>

namespace {

static constexpr Pic_gic::Gic_info exynos4_ext_gic =
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys = 0x10490000, .dist_size = 0x10000,
  .cpu_phys  = 0x10480000, .cpu_size  = 0x10000,
};

static constexpr Pic_gic::Gic_info exynos5_gic =
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys  = 0x10481000, .dist_size  = 0x1000,
  .cpu_phys   = 0x10482000, .cpu_size   = 0x1000,
  .cpu_h_phys = 0x10484000, .cpu_h_size = 0x2000,
  .cpu_v_phys = 0x10486000, .cpu_v_size = 0x1000,
};

static constexpr unsigned short comb_4412_irqs[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 107, 108, 48, 42 };

static constexpr unsigned short comb_4210_irqs[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static constexpr unsigned short comb_5xxx_irqs[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };

static constexpr Mgr_exynos::Irq_info wu_5xxx_irqs[] =
{
    { 1, 8 * 23 }, // combiner 23 irq 0
    { 1, 8 * 24 },
    { 1, 8 * 25 },
    { 1, 8 * 25 + 1},
    { 1, 8 * 26 },
    { 1, 8 * 26 + 1},
    { 1, 8 * 27 },
    { 1, 8 * 27 + 1},
    { 1, 8 * 28 },
    { 1, 8 * 28 + 1},
    { 1, 8 * 29 },
    { 1, 8 * 29 + 1},
    { 1, 8 * 30 },
    { 1, 8 * 30 + 1},
    { 1, 8 * 31 },
    { 1, 8 * 31 + 1},
    { 0, 64 }, // GIC SPI 32 -> 64
};

static constexpr Mgr_exynos::Irq_info wu_4_irqs[] =
{
    { 0, 48 }, // GIC SPI 16
    { 0, 49 },
    { 0, 50 },
    { 0, 51 },
    { 0, 52 },
    { 0, 53 },
    { 0, 54 },
    { 0, 55 },
    { 0, 56 },
    { 0, 57 },
    { 0, 58 },
    { 0, 59 },
    { 0, 60 },
    { 0, 61 },
    { 0, 62 },
    { 0, 63 },
    { 0, 64 }, // GIC SPI 32 -> 64
};


static constexpr Mgr_exynos::Info irq_info_4412 =
{
  .gic = exynos4_ext_gic,
  .gic_offset = 0x4000,
  .gic_irqs   = 160,
  .num_combiners = 20,
  .c_irqs = comb_4412_irqs,
  .wu_phys = 0x11000000,
  .wu_irqs = wu_4_irqs,
  .n_gpio = 2,
  .gpio = {
        { 0x11400000, 47 + 32, 13 * 8 },
        { 0x11000000, 46 + 32, 12 * 8 }
  }
};

static constexpr Mgr_exynos::Info irq_info_4210 =
{
  .gic = exynos4_ext_gic,
  .gic_offset = 0x8000,
  .gic_irqs   = 160,
  .num_combiners = 16,
  .c_irqs = comb_4210_irqs,
  .wu_phys = 0x11000000,
  .wu_irqs = wu_4_irqs,
  .n_gpio = 2,
  .gpio = {
        { 0x11400000, 47 + 32, 16 * 8 },
        { 0x11000000, 46 + 32,  9 * 8 }
  }
};

static constexpr Mgr_exynos::Info irq_info_5250 =
{
  .gic = exynos5_gic,
  .gic_irqs = 160,
  .num_combiners = 32,
  .c_irqs = comb_5xxx_irqs,
  .wu_phys = 0x11400000,
  .wu_irqs = wu_5xxx_irqs,
  .n_gpio = 4,
  .gpio = {
        { 0x11400000, 46 + 32, 13 * 8 },
        { 0x13400000, 45 + 32,  9 * 8 },
        { 0x10d10000, 50 + 32,  5 * 8 },
        { 0x03860000, 47 + 32,  1 * 8 }
  }
};

static constexpr Mgr_exynos::Info irq_info_5410 =
{
  .gic = exynos5_gic,
  .gic_irqs = 256,
  .num_combiners = 32,
  .c_irqs = comb_5xxx_irqs,
  .wu_phys = 0x11400000,
  .wu_irqs = wu_5xxx_irqs,
  .n_gpio = 4,
  .gpio = {
        { 0x11400000, 46 + 32, 21 * 8 },
        { 0x13400000, 45 + 32,  9 * 8 },
        { 0x10d10000, 50 + 32,  5 * 8 },
        { 0x03860000, 47 + 32,  1 * 8 }
  }
};


struct Syscon : Register_block<32>
{
  enum R
  {
    Prod_id = 0,
    Pkg_id  = 4,
  };
};

struct Exynos_pf : Platform_if_base
{
  Syscon syscon;

  Address scu_phys() override
  {
    return 0x10000000;
  }

  void init() override
  {
#if defined (CONFIG_ARM_MPCORE) || defined (CONFIG_ARM_CORTEX_A9) || defined (CONFIG_ARM_CORTEX_A5)
    if (IS_ENABLED(CONFIG_ARM_EM_NS) && Cpu::scu.available())
      Cpu::scu.r[Scu::R::Control].set(Scu::Control::Scu_standby);
#endif
  }

  void init_irqs() override
  {
    switch (Platform::soc_type())
      {
      case Platform::Soc_4210:
        Irq_mgr::mgr = new Boot_object<Mgr_exynos>(&irq_info_4210);
        break;
      case Platform::Soc_4412:
        Irq_mgr::mgr = new Boot_object<Mgr_exynos>(&irq_info_4412);
        break;
      case Platform::Soc_5250:
        Irq_mgr::mgr = new Boot_object<Mgr_exynos>(&irq_info_5250);
        break;
      case Platform::Soc_5410:
        Irq_mgr::mgr = new Boot_object<Mgr_exynos>(&irq_info_5410);
        break;
      default:
        panic("unknown exynos platform");
      }
  }

  void init_irqs_ap(Cpu_number cpu, bool resume) override
  {
    nonull_static_cast<Mgr_exynos *>(Irq_mgr::mgr)->init_ap(cpu, resume);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static Exynos_pf __pf;

}

