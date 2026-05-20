
#include <pic.h>
#include <gic_v2.h>
#include <irq_mgr_multi_chip.h>
#include <cascade_irq.h>
#include <kmem.h>
#include <pic-gic-helper.h>

#include <initcalls.h>
#include <types.h>
#include <gic.h>
#include <cascade_irq.h>
#include <globalconfig.h>

enum
{
  INTMODE_NEW_NO_DDC = 1 << 23,
};

#if defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_CORTEX_A9)

#include <cpu.h>
#include <io.h>
#include <platform.h>

inline void unlock_config()
{ Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock); }

inline void lock_config()
{ Platform::sys->write<Mword>(0x0, Platform::Sys::Lock); }

inline void configure_core()
{
  // Enable 'new' interrupt-mode, no DCC
  unlock_config();
  Platform::sys->modify<Mword>(INTMODE_NEW_NO_DDC, 0, Platform::Sys::Pld_ctrl1);
  lock_config();
}

#else

inline void configure_core()
{}

#endif

#if defined(CONFIG_PF_REALVIEW_PB11MP) || \
   (defined(CONFIG_PF_REALVIEW_EB) && \
     (defined(CONFIG_ARM_MPCORE) || \
      (defined(CONFIG_ARM_CORTEX_A9) && defined(CONFIG_MP))))

static Pic_gic::Gic_info const gics[] =
{
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = Mem_layout::Gic_dist_phys_base,
      .dist_size = 0x1000,
      .cpu_phys  = Mem_layout::Gic_cpu_phys_base,
      .cpu_size  = 0x100,
    },
    {
      .version = 2, .primary = false, .offset = 256,
      .dist_phys = Mem_layout::Gic1_dist_phys_base,
      .dist_size = 0x1000,
      .cpu_phys  = Mem_layout::Gic1_cpu_phys_base,
      .cpu_size  = 0x100
    }
};

FIASCO_INIT
void Pic::init()
{
  configure_core();
  typedef Irq_mgr_multi_chip<8> Mgr;
  Mgr *m = new Boot_object<Mgr>(4);
  Irq_mgr::mgr = m;

  Pic_gic::add_gic(m, gics[0]);
  Pic_gic::add_gic(m, gics[1]);

  Cascade_irq *casc_irq = new Boot_object<Cascade_irq>(m->chip(256).chip, &Gic_v2::cascade_hit);

  m->chip(0).chip->alloc(casc_irq, 42);
  casc_irq->unmask();
}

#else

static Pic_gic::Gic_info const gics[] =
{
    {
      .version = 2, .primary = true, .offset = 0,
#ifdef CONFIG_PF_REALVIEW_VEXPRESS_A9
      .dist_phys  = 0x1e001000, .dist_size  = 0x1000,
      .cpu_phys   = 0x1e000100, .cpu_size   = 0x100,
#else
      .dist_phys  = 0x2c001000, .dist_size  = 0x1000,
      .cpu_phys   = 0x2c002000, .cpu_size   = 0x100,
      .cpu_h_phys = 0x2c004000, .cpu_h_size = 0x1000,
      .cpu_v_phys = 0x2c006000, .cpu_v_size = 0x1000,
#endif
    }
};

FIASCO_INIT
void Pic::init()
{
  configure_core();

  typedef Irq_mgr_multi_chip<8> Mgr;
  Mgr *m = new Boot_object<Mgr>(1);
  Irq_mgr::mgr = m;
  Pic_gic::add_gic(m, gics[0]);
}

#endif

void Pic::init_ap(Cpu_number cpu, bool resume)
{
  for (auto g: gics)
    static_cast<Gic*>(Irq_mgr::mgr->chip(g.offset).chip)->init_ap(cpu, resume);
}


