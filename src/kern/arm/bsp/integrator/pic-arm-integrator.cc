#include <pic.h>
#include <initcalls.h>

#include <cassert>
#include <irq_chip_generic.h>
#include <irq_mgr.h>
#include <mmio_register_block.h>
#include <kmem.h>
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
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  void ack(Mword) override { /* ack is empty */ }

  Irq_chip_arm_integr()
  : Irq_chip_gen(32),
    Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base, 0x100))
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

#ifdef CONFIG_JDB
  char const *chip_type() const override
  { return "Integrator"; }
#endif
};

static Static_object<Irq_mgr_single_chip<Irq_chip_arm_integr> > mgr;

FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

extern "C" void irq_handler();
void irq_handler()
{ mgr->c.handle_multi_pending<Irq_chip_arm_integr>(0); }


#if 0 // for ARM_EM_TZ + TZ_VM

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}
#endif

