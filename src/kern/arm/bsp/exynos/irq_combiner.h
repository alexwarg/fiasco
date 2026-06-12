#pragma once

#include <irq_chip.h>
#include <irq_chip_generic.h>
#include <mmio_register_block.h>


class Combiner_chip : public Irq_chip_gen, private Mmio_register_block
{
public:
  IRQ_CHIP_DBG_INFO("Comb");

  enum
  {
    Enable_set    = 0,
    Enable_clear  = 4,
    Status        = 12,
  };

  enum
  {
    No_pending = ~0UL,
  };

  Mword offset(unsigned irq) const { return (irq >> 2) * 0x10; }

  static unsigned shift(int irq)
  { return (irq % 4) * 8; }

  static Mword bytemask(int irq)
  { return 0xffUL << shift(irq); }

  Mword status(int irq) const
  { return read<Mword>(offset(irq) + Status) & bytemask(irq); }

  void mask(Mword i)
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_clear); }

  void mask_and_ack(Mword i)
  { Combiner_chip::mask(i); }

  void ack(Mword) {}

  void set_cpu(Mword, Cpu_number) {}

  int set_mode(Mword, Mode)
  { return 0; }

  bool is_edge_triggered(Mword) const
  { return false; }

  void unmask(Mword i)
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_set); }

  void init_irq(int irq) const
  { write<Mword>(bytemask(irq), offset(irq) + Enable_clear); }

  Unsigned32 pending(unsigned cnr)
  {
    unsigned v = status(cnr) >> shift(cnr);
    if (v)
      return (cnr * 8) + __builtin_ctz(v);
    return No_pending;
  }

  explicit Combiner_chip(void *mmio_va, unsigned num_chips)
  : Irq_chip_gen(num_chips * 8),
    Mmio_register_block(mmio_va)
  {
    // 0..39, 51, 53
    for (unsigned i = 0; (i < 40) && (i < num_chips); ++i)
      init_irq(i);

    if (num_chips > 51)
      init_irq(51);

    if (num_chips > 53)
      init_irq(53);
  }
};

class Combiner_cascade_irq : public Irq_base
{
public:
  Combiner_cascade_irq(unsigned nr, Combiner_chip *chld)
  : _combiner_nr(nr), _child(chld)
  { set_hit(&handler_wrapper<Combiner_cascade_irq>); }

  void switch_mode(bool) override {}
  unsigned irq_nr_base() const { return _combiner_nr * 8; }

  void handle(Upstream_irq const *u)
  {
    Unsigned32 num = _child->pending(_combiner_nr);
    Upstream_irq ui(this, u);

    if (num != Combiner_chip::No_pending)
      _child->irq(num)->hit(&ui);
  }

private:
  unsigned _combiner_nr;
  Combiner_chip *_child;
};


