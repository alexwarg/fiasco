#pragma once

#include <irq_chip_generic.h>
#include <mmio_register_block.h>

class Irq_chip_ingenic : public Irq_chip_gen
{
public:
  Irq_chip_ingenic(Address mmio)
  : Irq_chip_gen(32), _r(mmio)
  {
    _r[R_mask] = 0xffffffff;
  }

  Unsigned32 pending() const
  {
    return _r[R_pending];
  }

  bool handle_pending(Upstream_irq const *ui)
  {
    unsigned s = __builtin_ffs(pending());
    if (!s)
      return false;

    handle_irq<Irq_chip_ingenic>(s - 1, ui);
    return true;
  }

  void unmask(Mword pin) override
  {
    _r[R_clear_mask] = 1UL << pin;
  }

  void mask(Mword pin) override
  {
    _r[R_set_mask] = 1UL << pin;
  }

  void mask_and_ack(Mword pin) override
  {
    _r[R_set_mask] = 1UL << pin;
  }

  void ack(Mword) override
  {}

  int set_mode(Mword, Mode) override
  {
    return 0;
  }

  bool is_edge_triggered(Mword) const override
  {
    return false;
  }

  void set_cpu(Mword, Cpu_number) override
  {}

#ifdef CONFIG_JDB
  char const *chip_type() const override
  {
    return "Ingenic";
  }
#endif

private:
  enum : Address
  {
    R_status     = 0x00,
    R_mask       = 0x04,
    R_set_mask   = 0x08,
    R_clear_mask = 0x0c,
    R_pending    = 0x10,
  };

  Register_block<32> _r;
};

