#pragma once

#include "irq_mgr.h"
#include "globalconfig.h"

#include <cstdio>

/**
 * Flexible IRQ manager (manages multiple IRQ chips).
 *
 * This manager allow to manage up to `MAX_CHIPS` IRQ chips, where each chip
 * gets a contiguous range of externally visible (global IRQ numbers)
 * assigned.  The ranges of the different chips must not overlap. However each
 * chip can start at an arbitrary 16bit IRQ number.
 */
template<unsigned MAX_CHIPS>
class Irq_mgr_flex : public Irq_mgr_dyn
{
public:
  unsigned max_chips() const { return MAX_CHIPS; }

  unsigned nr_irqs() const override
  { return _max_irq; }

  unsigned nr_msis() const override
  { return 0; }

  Irq chip(Mword irqnum) const override
  {
    for (unsigned i = 0; i < _used; ++i)
      if (irqnum < _chips[i].end)
        return Irq(_chips[i].chip, irqnum - _chips[i].start);

    return Irq();
  }

  int add_msi_chip(Irq_chip_icu_msi *chip) override
  { return -E_too_many_chips; }

  /**
   * Add a chip starting its range at `pos`.
   * \param chip  The chip to add.
   * \param pos   The first global IRQ number assigned to pin 0 of `chip`.
   *              use `-1` to assign the next free global IRQ number.
   *
   * This function checks for overlaps and returns an error string in case
   * of any conflicts.
   */
  int add_chip(int pos, Irq_chip_icu *chip, int pins = -1) override
  {
    if (_used >= MAX_CHIPS)
      return -E_too_many_chips;

    unsigned n = pins >= 0 ? pins : chip->nr_irqs();
    if (n == 0)
      return -E_zero_pins;

    if (pos < 0)
      pos = _max_irq;

    if (pos >= _max_irq)
      {
        // add as last
        auto &e = _chips[_used++];
        e.start = pos;
        _max_irq = pos + n;
        e.end   = _max_irq;
        e.chip  = chip;
        return 0;
      }

    Chip *spot = 0;
    for (unsigned x = 0; x < _used; ++x)
      {
        spot = &_chips[x];

        if (pos >= spot->end)
          continue;

        if (pos + n >= spot->start)
          return -E_irqs_in_use;
      }

    for (Chip *x = &_chips[_used]; x >= spot; --x)
      x[1] = x[0];

    ++_used;

    spot->start = pos;
    spot->end   = pos + n;
    spot->chip = chip;

    if (pos + n > _max_irq)
      _max_irq = pos + n;

    return 0;
  }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    Irq_chip_icu *done[MAX_CHIPS];
    unsigned done_num = 0;

    for (auto const *e = _chips; e != _chips + _used; ++e)
      {
        if (!e->chip)
          continue;

        bool found = false;
        for (auto const *d = done; d != done + done_num; ++d)
          if (*d == e->chip)
            {
              found = true;
              break;
            }

        if (found)
          continue;

        e->chip->init_ap(cpu, resume);
        done[done_num++] = e->chip;
      }
  }

  void print_infos();

private:
  struct Chip
  {
    unsigned short start;
    unsigned short end;
    Irq_chip_icu *chip;
  };
  unsigned _used = 0;
  unsigned _max_irq = 0;
  Chip _chips[MAX_CHIPS];
};

#if ! defined (CONFIG_JDB)

template<unsigned MAX_CHIPS>
void
Irq_mgr_flex<MAX_CHIPS>::print_infos()
{
  for (auto *e = _chips; e != _chips + _used; ++e)
    printf("  %3d-%3d: @%p\n", e->start, e->end - 1, e->chip);
}

#else // !CONFIG_JDB

template<unsigned MAX_CHIPS>
void
Irq_mgr_flex<MAX_CHIPS>::print_infos()
{
  for (auto *e = _chips; e != _chips + _used; ++e)
    printf("  %3d-%3d: %s\n", e->start, e->end - 1, e->chip->chip_type());
}

#endif // ! CONFIG_JDB
