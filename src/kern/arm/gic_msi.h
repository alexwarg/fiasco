
#pragma once

#include <gic_its.h>
#include <irq_chip_generic.h>
#include <globalconfig.h>

class Gic_msi : public Irq_chip_gen
{
private:
  Gic_its *_its;

public:
  void init(Gic_its *its, unsigned nrmsis)
  {
    _its = its;
    Irq_chip_gen::init(nrmsis);
  }

  int set_mode(Mword, Mode) override
  { return 0; }

  bool is_edge_triggered(Mword) const override
  { return true; }

  void mask(Mword pin) override
  {
    _its->mask_lpi(pin);
  }

  void ack(Mword pin) override
  {
    _its->ack_lpi(pin);
  }

  void mask_and_ack(Mword pin) override
  {
    assert (cpu_lock.test());
    mask(pin);
    ack(pin);
  }

  void unmask(Mword pin) override
  {
    _its->unmask_lpi(pin);
  }

  void set_cpu(Mword pin, Cpu_number cpu) override
  {
    _its->assign_lpi_to_cpu(pin, cpu);
  }

  void unbind(Irq_base *irq) override
  {
    _its->free_lpi(irq->pin());
    Irq_chip_gen::unbind(irq);
  }

  int msg(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *inf)
  {
    if (pin >= nr_irqs())
      return -L4_err::ERange;

    return _its->bind_lpi_to_device(pin, src, inf);
  }

#ifdef CONFIG_JDB
  char const *chip_type() const override
  { return "GIC-MSI"; }
#endif
};


