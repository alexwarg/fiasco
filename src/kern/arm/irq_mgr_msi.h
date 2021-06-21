#pragma once

#include <irq_mgr.h>
#include <globalconfig.h>

template<typename CHIP, typename MSI_CHIP>
class Irq_mgr_msi : public Irq_mgr
{
private:
  CHIP *_chip;
#ifdef CONFIG_ARM_GIC_MSI
  enum { Msi_flag = 0x80000000 };
  MSI_CHIP *_msi_chip;
#endif

public:
  unsigned nr_irqs() const override
  { return _chip->CHIP::nr_irqs(); }

#ifdef CONFIG_ARM_GIC_MSI
  Irq_mgr_msi(CHIP *chip, MSI_CHIP *msi_chip)
  : _chip(chip), _msi_chip(msi_chip)
  {}

  Irq chip(Mword irq) const override
  {
    if (irq & Msi_flag)
      return _msi_chip ? Irq(_msi_chip, irq & ~Msi_flag) : Irq();
    else
      return Irq(_chip, irq);
  }

  unsigned nr_msis() const override
  { return _msi_chip ? _msi_chip->MSI_CHIP::nr_irqs() : 0; }

  int msg(Mword irq, Unsigned64 src, Msi_info *inf) const override
  {
    if ((irq & Msi_flag) && _msi_chip)
      return _msi_chip->MSI_CHIP::msg(irq & ~Msi_flag, src, inf);
    else
      return -L4_err::ERange;
  }

#else
  Irq_mgr_msi(CHIP *chip, MSI_CHIP *) : _chip(chip)
  {}

  Irq chip(Mword irq) const override
  { return Irq(_chip, irq); }

  unsigned nr_msis() const override
  { return 0; }

  int msg(Mword, Unsigned64, Msi_info *) const override
  { return -L4_err::ENosys; }
#endif
};
