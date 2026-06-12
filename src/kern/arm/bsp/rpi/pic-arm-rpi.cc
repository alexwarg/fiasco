
#include <irqs_rpi.h>
#include <boot_alloc.h>
#include <irq_entry.h>
#include <globalconfig.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <irq_mgr.h>

#ifndef CONFIG_ARM_GIC
#include <irq_chip_bcm.h>

using Irq_mgr_rpi = Irq_mgr_single_chip<Irq_chip_bcm>;

#if defined(CONFIG_PF_RPI_RPI1) || defined (CONFIG_PF_RPI_RPIZW)

static void bcm_irq_handler()
{
  auto mgr = nonull_static_cast<Irq_mgr_rpi *>(Irq_mgr::mgr);
  mgr->c.irq_handler();
}

Irq_mgr *
Arm_rpi::create_irq_mgr_bcm(bool)
{
  auto m = new Boot_object<Irq_mgr_rpi>(96, Kmem_mmio::map(Mem_layout::Pic_phys_base, 0x100));
  Irq_mgr::mgr = m;
  Arm_irqs::set_irq_handler(bcm_irq_handler);
  return m;
}

#endif

#if defined(CONFIG_PF_RPI_RPI2) || defined(CONFIG_PF_RPI_RPI3)

#include <timer.h>
#include <timer_tick.h>
#include <arm_control.h>

#ifdef CONFIG_MP

#include <ipi.h>
#include <thread.h>
#include <sched.h>
#include <arm_ipis.h>

inline void handle_ipis()
{
  Ipi::Message m = Ipi::pending();
  switch (m)
    {
    case Ipi::Request: Sched<>::handle_remote_requests_irq(); break;
    case Ipi::Global_request: Thread::handle_global_remote_requests_irq(); break;
    case Ipi::Debug: Arm_ipis::Debug::handler(); break;
    case Ipi::Timer: Arm_ipis::Timer::handler(); break;
    };
}
#else
inline void handle_ipis()
{}
#endif


static void bcm2836_irq_handler()
{
  auto mgr = nonull_static_cast<Irq_mgr_rpi *>(Irq_mgr::mgr);
  while (Unsigned32 pending = Arm_control::o()->irqs_pending())
    {
      if (pending & (1 << 4)) // mailbox 0
        handle_ipis();

      if (pending & (1 << Timer::irq()))
        Timer_tick::handler_static_ack();

      if (pending & 0x100)
        mgr->c.irq_handler();
    }
}

Irq_mgr *
Arm_rpi::create_irq_mgr_bcm2836(bool)
{
  auto m = new Boot_object<Irq_mgr_rpi>(96, Kmem_mmio::map(Mem_layout::Pic_phys_base, 0x100));
  Irq_mgr::mgr = m;
  Arm_irqs::set_irq_handler(bcm2836_irq_handler);
  Arm_control::init();
  return m;
}

#endif

#if 0 // ARM_EM_TZ + TZ_VM
#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}
#endif

#else // CONFIG_ARM_GIC

#include <gic_v2.h>
#include <irq_mgr_multi_chip.h>
#include <kmem.h>
#include <pic-gic-helper.h>

#ifdef CONFIG_BIT32
#include <arm_control.h>
#endif

Pic_gic::Gic_info const Pic_gic::primary_gic_info =
{
  .version = 2, .primary = true, .offset = 0,
  .dist_phys  = 0xff841000, .dist_size = 0x1000,
  .cpu_phys   = 0xff842000, .cpu_size = 0x100,
  .cpu_h_phys = 0xff844000, .cpu_h_size = 0x1000,
  .cpu_v_phys = 0xff846000, .cpu_v_size = 0x1000,
};

Irq_mgr *
Arm_rpi::create_irq_mgr_gic(bool)
{
  Pic_gic::add_gic(Pic_gic::primary_gic_info);
#ifdef CONFIG_BIT32
  Arm_control::init();
#endif
  return Irq_mgr::mgr;
}

#endif // CONFIG_ARM_GIC
