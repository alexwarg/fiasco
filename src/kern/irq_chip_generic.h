#pragma once

#include "irq_chip.h"


class Irq_chip_gen : public Irq_chip_icu
{
public:
  Irq_chip_gen() = default;
  explicit Irq_chip_gen(unsigned nirqs) { init(nirqs); }

  unsigned nr_irqs() const override
  {
    return _nirqs;
  }

  Irq_base *irq(Mword pin) const override;
  bool alloc(Irq_base *irq, Mword pin, bool init = true) override;
  void unbind(Irq_base *irq) override;
  bool reserve(Mword pin) override;

  void init(unsigned nirqs);

private:
  unsigned _nirqs;
  Irq_base **_irqs;
};

