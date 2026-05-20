#pragma once

#include <irq_mgr.h>
#include <boot_alloc.h>
#include <l4_types.h>

#include <cassert>
#include <cstring>

template< unsigned Bits_per_entry >
class Irq_mgr_multi_chip : public Irq_mgr_dyn
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

  int add_chip(int irq_base, Irq_chip_icu *c, int pins = -1) override
  {
    if ((irq_base & ~(~0UL << Bits_per_entry)) != 0)
      return -E_unaligned_base;

    if (!c)
      return -E_no_chip;

    if (pins < 0)
      pins = c->nr_irqs();

    if (pins <= 0)
      return -E_zero_pins;

    unsigned idx = irq_base >> Bits_per_entry;
    unsigned num = (pins + (1UL << Bits_per_entry) - 1) >> Bits_per_entry;

    if (idx >= _nchips)
      return -E_range;

    if (idx + num > _nchips)
      num = _nchips - idx;

    for (unsigned i = idx; i < idx + num; ++i)
      if (_chips[i].chip)
        return -E_irqs_in_use;

    for (unsigned i = idx; i < idx + num; ++i)
      {
        _chips[i].chip = c;
        _chips[i].offset = irq_base;
      }

    return 0;
  }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    Irq_chip_icu *last = nullptr;
    for (auto const &c: cxx::static_vector<Chip>(_chips, _nchips))
      if (c.chip && c.chip != last)
        {
          c.chip->init_ap(cpu, resume);
          last = c.chip;
        }
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

