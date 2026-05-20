
#include <pic.h>
#include <initcalls.h>
#include <globalconfig.h>
#include <kmem.h>
#include <irq_mgr.h>

#ifndef CONFIG_ARM_GIC
#include <irq_chip_bcm.h>

static Static_object<Irq_mgr_single_chip<Irq_chip_bcm> > mgr;
extern "C" void irq_handler();

#if defined(CONFIG_PF_RPI_RPI1) || defined (CONFIG_PF_RPI_RPIZW)
FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct(96, Kmem::mmio_remap(Mem_layout::Pic_phys_base, 0x100));
}

void irq_handler()
{ mgr->c.irq_handler(); }

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


FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct(96, Kmem::mmio_remap(Mem_layout::Pic_phys_base, 0x100));
  Arm_control::init();
}

void irq_handler()
{
  while (Unsigned32 pending = Arm_control::o()->irqs_pending())
    {
      if (pending & (1 << 4)) // mailbox 0
        handle_ipis();

      if (pending & (1 << Timer::irq()))
        Timer_tick::handler_all(0, 0);

      if (pending & 0x100)
        mgr->c.irq_handler();
    }
}

#endif

void
Pic::init_ap(Cpu_number, bool)
{}

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

FIASCO_INIT
void
Pic::init()
{
  Pic_gic::add_gic(Pic_gic::primary_gic_info);
#ifdef CONFIG_BIT32
  Arm_control::init();
#endif
}

void Pic::init_ap(Cpu_number cpu, bool resume)
{
  Gic::primary->init_ap(cpu, resume);
}

#endif // CONFIG_ARM_GIC

