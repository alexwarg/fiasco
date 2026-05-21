#include <irqs_imx.h>

#include <boot_alloc.h>
#include <irq_entry.h>
#include <kmem.h>

#include <cassert>
#include <irq_chip_generic.h>
#include <irq_mgr.h>
#include <mmio_register_block.h>

#include <globalconfig.h>

class Irq_chip_imx_icoll : public Irq_chip_gen
{
private:
  enum
  {
    HW_ICOLL_VECTOR         =   0x0,
    HW_ICOLL_LEVELACK       =  0x10,
    HW_ICOLL_CTRL           =  0x20,
    HW_ICOLL_INTERRUPT0     = 0x120,
    HW_ICOLL_INTERRUPT0_SET = 0x124,
    HW_ICOLL_INTERRUPT0_CLR = 0x128,

    HW_ICOLL_CTRL_IRQ_FINAL_ENABLE = 1 << 16,

    HW_ICOLL_INTERRUPT_ENABLE      = 1 << 2,

    PRIO_LEVEL = 0,
  };

  Register_block<32> _reg;

public:
  IRQ_CHIP_DBG_INFO("i.MX-icoll IRQ");

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  void ack(Mword) override { _reg[HW_ICOLL_LEVELACK] = 1 << PRIO_LEVEL; }

  void mask(Mword irq) override
  {
    assert(cpu_lock.test());
    _reg[HW_ICOLL_INTERRUPT0_CLR + irq * 0x10] = HW_ICOLL_INTERRUPT_ENABLE;
  }

  void mask_and_ack(Mword irq) override
  {
    assert(cpu_lock.test());
    mask(irq);
    ack(irq);
  }

  void unmask(Mword irq) override
  {
    assert (cpu_lock.test());
    _reg[HW_ICOLL_INTERRUPT0_SET + irq * 0x10] = HW_ICOLL_INTERRUPT_ENABLE;
  }

  Irq_chip_imx_icoll()
  : Irq_chip_gen(128), _reg(Kmem::mmio_remap(Mem_layout::Pic_phys_base, 0x1000))
  {
    _reg[HW_ICOLL_CTRL] = 0;

    for (unsigned i = 0; i < 128; ++i)
      _reg[HW_ICOLL_INTERRUPT0 + i * 0x10] = PRIO_LEVEL; // Normal interrupt

    _reg[HW_ICOLL_CTRL] = HW_ICOLL_CTRL_IRQ_FINAL_ENABLE;
  }

  Unsigned32 pending()
  {
    return _reg[HW_ICOLL_VECTOR] >> 2;
  }

  void irq_handler()
  {
    Unsigned32 p = pending();
    _reg[HW_ICOLL_VECTOR] = p; // write anything
    handle_irq<Irq_chip_imx_icoll>(p, 0);
  }
};

using Irq_mgr_imx_icoll = Irq_mgr_single_chip<Irq_chip_imx_icoll>;

static void imx_icoll_irq_handler()
{
  auto mgr = nonull_static_cast<Irq_mgr_imx_icoll *>(Irq_mgr::mgr);
  mgr->c.irq_handler();
}

Irq_mgr *
Arm_imx_icoll::create_irq_mgr(bool)
{
  auto m = new Boot_object<Irq_mgr_imx_icoll>();
  Irq_mgr::mgr = m;
  Arm_irqs::set_irq_handler(imx_icoll_irq_handler);
  return m;
}
