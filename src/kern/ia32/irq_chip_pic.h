#pragma once

#include <i8259.h>
#include <io.h>
#include <irq_chip_ia32.h>
#include <irq_mgr.h>

/**
 * IRQ Chip based on the IA32 legacy PIC.
 *
 * 16 Vectors starting from Base_vector are statically assigned.
 */
class Irq_chip_ia32_pic :
  public Irq_chip_i8259<Io>,
  private Irq_chip_ia32,
  private Irq_mgr
{
  friend class Irq_chip_ia32;
public:
  Irq_chip_ia32_pic();

  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

  bool alloc(Irq_base *irq, Mword irqn, bool init = true) override;
  void unbind(Irq_base *irq) override;
  unsigned nr_irqs() const override;
  unsigned nr_msis() const override;

private:
  enum { Base_vector = 0x20 };

  Irq chip(Mword irq) const override;
};

