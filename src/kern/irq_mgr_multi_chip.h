#pragma once

#include "irq_mgr.h"
#include "boot_alloc.h"

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

    return Irq(ci->chip, irqnum & ci->mask);
  }

  void add_chip(unsigned irq_base, Irq_chip_icu *c, unsigned pins)
  {
    // check if the base is properly aligned
    assert ((irq_base & ~(~0UL << Bits_per_entry)) == 0);

    unsigned idx = irq_base >> Bits_per_entry;
    unsigned num = (pins + (1UL << Bits_per_entry) - 1) >> Bits_per_entry;

    unsigned mask = ~0U;
    while (mask & (pins - 1))
      mask <<= 1;

    assert (mask);
    mask = ~mask;

    // base irq must be aligned according to the number of pins
    assert (!(irq_base & mask));

    assert (num);
    assert (idx < _nchips);
    assert (idx + num <= _nchips);

    for (unsigned i = idx; i < idx + num; ++i)
      {
        assert (!_chips[i].chip);
        _chips[i].chip = c;
        _chips[i].mask = mask;
      }
  }

private:
  struct Chip
  {
    unsigned mask;
    Irq_chip_icu *chip;
  };

  unsigned _nchips;
  Chip *_chips;
};

