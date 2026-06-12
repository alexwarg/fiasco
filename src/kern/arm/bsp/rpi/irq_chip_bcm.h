#pragma once

#include <mmio_register_block.h>
#include <irq_chip_generic.h>
#include <types.h>
#include <arithmetic.h>
#include <globalconfig.h>
#include <cassert>

class Irq_chip_bcm : public Irq_chip_gen, Mmio_register_block
{
public:
  IRQ_CHIP_DBG_INFO("RPI IRQ");

  enum
  {
    Irq_basic_pending  = 0x0,
    Irq_pending_1      = 0x4,
    Irq_pending_2      = 0x8,
    Fiq_control        = 0xc,
    Enable_IRQs_1      = 0x10,
    Enable_IRQs_2      = 0x14,
    Enable_Basic_IRQs  = 0x18,
    Disable_IRQs_1     = 0x1c,
    Disable_IRQs_2     = 0x20,
    Disable_Basic_IRQs = 0x24,
  };

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  void ack(Mword) override { /* ack is empty */ }

  Irq_chip_bcm(unsigned nr_irqs, void *base)
  : Irq_chip_gen(nr_irqs), Mmio_register_block(base)
  {
    write<Unsigned32>(~0U, Disable_Basic_IRQs);
    write<Unsigned32>(~0U, Disable_IRQs_1);
    write<Unsigned32>(~0U, Disable_IRQs_2);
  }

  void mask(Mword irq) override
  {
    assert(cpu_lock.test());
    write<Unsigned32>(1 << (irq & 0x1f), Disable_IRQs_1 + ((irq & 0x60) >> 3));
  }

  void mask_and_ack(Mword irq) override
  {
    mask(irq);
    // ack is empty
  }

  void unmask(Mword irq) override
  {
    assert(cpu_lock.test());
    write<Unsigned32>(1 << (irq & 0x1f), Enable_IRQs_1 + ((irq & 0x60) >> 3));
  }

  void irq_handler()
  {
    for (;;)
      {
        unsigned b = 64;
        Unsigned32 p = read<Unsigned32>(Irq_basic_pending);

        if (p & 0x100)
          {
            b = 0;
            p = read<Unsigned32>(Irq_pending_1);
          }
        else if (p & 0x200)
          {
            b = 32;
            p = read<Unsigned32>(Irq_pending_2);
          }
        else if (p)
          {
            unsigned m = p & 0x1ffc00;
            if (m)
              {
                m >>= 10;
                char map[11] = { 7, 9, 10, 18, 19, 53, 54, 55, 56, 57, 62 };

                handle_irq<Irq_chip_bcm>(map[cxx::log2u(m)], 0);
                continue;
              }
          }

        if (p)
          handle_irq<Irq_chip_bcm>(b + cxx::log2u(p), 0);
        else
          return;
      }
  }
};

