#include <irqs_integrator.h>
#include <boot_alloc.h>
#include <irq_entry.h>

#include <cassert>
#include <irq_chip_generic.h>
#include <irq_mgr.h>
#include <mmio_register_block.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <globalconfig.h>

class Irq_chip_arm_integr : public Irq_chip_gen, Mmio_register_block
{
private:
  enum
  {
    IRQ_STATUS       = 0x00,
    IRQ_ENABLE_SET   = 0x08,
    IRQ_ENABLE_CLEAR = 0x0c,

    FIQ_ENABLE_CLEAR = 0x2c,

    PIC_START = 0,
    PIC_END   = 31,
  };

public:
  IRQ_CHIP_DBG_INFO("Integrator");

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  void ack(Mword) override { /* ack is empty */ }

  Irq_chip_arm_integr()
  : Irq_chip_gen(32),
    Mmio_register_block(Kmem_mmio::map(Mem_layout::Pic_phys_base, 0x100))
  {
    write<Mword>(0xffffffff, IRQ_ENABLE_CLEAR);
    write<Mword>(0xffffffff, FIQ_ENABLE_CLEAR);
  }

  void mask(Mword irq) override
  {
    assert(cpu_lock.test());
    write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_CLEAR);
  }

  void mask_and_ack(Mword irq) override
  {
    assert(cpu_lock.test());
    write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_CLEAR);
    // ack is empty
  }

  void unmask(Mword irq) override
  {
    assert(cpu_lock.test());
    write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_SET);
  }

  Unsigned32 pending()
  {
    return read<Mword>(IRQ_STATUS);
  }
};

using Irq_mgr_integrator = Irq_mgr_single_chip<Irq_chip_arm_integr>;

static void integrator_irq_handler()
{
  auto mgr = nonull_static_cast<Irq_mgr_integrator *>(Irq_mgr::mgr);
  mgr->c.handle_multi_pending<Irq_chip_arm_integr>(0);
}

Irq_mgr *
Arm_integrator::create_irq_mgr(bool)
{
  auto m = new Boot_object<Irq_mgr_integrator>();
  Irq_mgr::mgr = m;
  Arm_irqs::set_irq_handler(integrator_irq_handler);
  return m;
}

#if 0 // for ARM_EM_TZ + TZ_VM

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}
#endif
