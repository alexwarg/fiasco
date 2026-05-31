#pragma once

#include <irq_mgr_multi_chip.h>
#include <globalconfig.h>

class Irq_mgr_multi_msi : public Irq_mgr_multi_chip<9>
{
private:
#ifdef CONFIG_ARM_GIC_MSI
  enum { Msi_flag = 0x80000000 };
  Irq_chip_icu_msi *_msi_chip = nullptr;
#endif

public:
  using Irq_mgr_multi_chip<9>::Irq_mgr_multi_chip;
#ifdef CONFIG_ARM_GIC_MSI
  int add_msi_chip(Irq_chip_icu_msi *chip) override
  {
    if (_msi_chip)
      return -E_too_many_chips;
    if (!chip)
      return -E_no_chip;

    _msi_chip= chip;
    return 0;
  }

  Irq chip(Mword irq) const override
  {
    if (irq & Msi_flag)
      return _msi_chip ? Irq(_msi_chip, irq & ~Msi_flag) : Irq();
    else
      return Irq_mgr_multi_chip::chip(irq);
  }

  unsigned nr_msis() const override
  { return _msi_chip ? _msi_chip->nr_irqs() : 0; }

  int msg(Mword irq, Unsigned64 src, Msi_info *inf) const override
  {
    if ((irq & Msi_flag) && _msi_chip)
      return _msi_chip->msg(irq & ~Msi_flag, src, inf);
    else
      return -L4_err::ERange;
  }
#endif
};

