#pragma once

#include <irq_mgr.h>
#include <boot_alloc.h>
#include <l4_types.h>

#include <cassert>
#include <cstring>

template< unsigned Bits_per_entry >
class Irq_mgr_multi_chip : public Irq_mgr
{
public:
  unsigned nr_irqs() const override { return _nchips << Bits_per_entry; }
  unsigned nr_msis() const override { return 0; }

  explicit Irq_mgr_multi_chip(unsigned chips) noexcept
    : _nchips(chips),
      _chips(new Boot_object<Chip>[chips]())
  {}

  Irq chip(Mword irqnum) const override
  {
    unsigned c = irqnum >> Bits_per_entry;
    if (c >= _nchips)
      return Irq();

    Chip *ci = _chips + c;

    return Irq(ci->chip, irqnum - ci->offset);
  }

  int add_chip(unsigned irq_base, Irq_chip_icu *c, unsigned pins)
  {
    if ((irq_base & ~(~0UL << Bits_per_entry)) != 0)
      return -L4_err::EInval;

    if (!c)
      return -L4_err::EInval;

    if (pins == 0)
      return -L4_err::EInval;

    unsigned idx = irq_base >> Bits_per_entry;
    unsigned num = (pins + (1UL << Bits_per_entry) - 1) >> Bits_per_entry;

    if (idx + num > _nchips)
      return -L4_err::ERange;

    for (unsigned i = idx; i < idx + num; ++i)
      if (_chips[i].chip)
        return -L4_err::EExists;

    for (unsigned i = idx; i < idx + num; ++i)
      {
        _chips[i].chip = c;
        _chips[i].offset = irq_base;
      }

    return 0;
  }

private:
  struct Chip
  {
    unsigned offset;
    Irq_chip_icu *chip;
  };

  unsigned _nchips;
  Chip *_chips;
};

