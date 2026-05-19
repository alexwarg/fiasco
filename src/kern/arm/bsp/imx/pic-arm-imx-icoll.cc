#include <pic.h>

#include <initcalls.h>
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

static Static_object<Irq_mgr_single_chip<Irq_chip_imx_icoll> > mgr;


FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

extern "C" void irq_handler();
void irq_handler()
{ mgr->c.irq_handler(); }

