#pragma once

#include <mmio_register_block.h>
#include <irq_chip_generic.h>
#include <spin_lock.h>

class Gt64120{
public:
  enum class R
  {
    Cpu_iface_conf         = 0x000,
    Pci0_sync              = 0x0c0, // ro
    Pci1_sync              = 0x0c8, // ro

    Multi_gt               = 0x120,

    Pci0_command           = 0xc00,
    Pci0_to_rtry           = 0xc04,
    Int_cause              = 0xc18,
    Cpu_int_mask           = 0xc1c,
    Pci0_int_cause_mask    = 0xc24,
    Int_cause_hi           = 0xc98,
    Cpu_int_mask_hi        = 0xc9c,
    Pci0_int_cause_mask_hi = 0xca4,
    Pci1_cfg_addr          = 0xcf0,
    Pci1_cfg_data          = 0xcf4,
    Pci0_cfg_addr          = 0xcf8,
    Pci0_cfg_data          = 0xcfc,
    Pci1_iack              = 0xc30, // ro
    Pci0_iack              = 0xc34, // ro
  };

  typedef Register_block<32, R> Gt_regs;

  Gt64120(void *virt_gt_regs, void *virt_pci_io)
  : _gt_regs(virt_gt_regs),
    _pci_io(virt_pci_io)
  {}

  Mmio_register_block const *pci_io() const { return &_pci_io; }
  Gt_regs const &gt_regs() const { return _gt_regs; }

protected:
  Gt_regs _gt_regs;
  Mmio_register_block _pci_io;
};

class Gt64120_irq : public Irq_chip_gen, public Gt64120
{
public:
  Gt64120_irq(void *virt_gt_regs, void *virt_pci_io)
    : Gt64120(virt_gt_regs, virt_pci_io)
  {
    Irq_chip_gen::init(29);
    _gt_regs[R::Cpu_int_mask] = 0;
    _gt_regs[R::Int_cause] = 0;
  }

  void unmask(Mword pin) override
  {
    auto g = lock_guard(_irq_lock);
    _gt_regs[R::Cpu_int_mask].set(1UL << (pin + 1));
  }

  void mask(Mword pin) override
  {
    auto g = lock_guard(_irq_lock);
    _gt_regs[R::Cpu_int_mask].clear(1UL << (pin + 1));
  }

  void mask_and_ack(Mword pin) override
  {
    auto g = lock_guard(_irq_lock);
    _gt_regs[R::Cpu_int_mask].clear(1UL << (pin + 1));
    // NOTE: this is according to the spec, writing a 0
    // for the IRQ to ack and all ones for the other bits.
    // However, Linux uses read-modify-write for that, what
    // creates a reace condition IMHO.
    _gt_regs[R::Int_cause] = ~(1UL << (pin + 1));
  }

  void ack(Mword pin) override
  {
    // This is a simple write, so no lock used

    // NOTE: this is according to the spec, writing a 0
    // for the IRQ to ack and all ones for the other bits.
    // However, Linux uses read-modify-write for that, what
    // creates a reace condition IMHO.
    _gt_regs[R::Int_cause] = ~(1UL << (pin + 1));
  }

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
    return "GT64120";
  }
#endif

private:
  Spin_lock<> _irq_lock{Spin_lock<>::Unlocked};
};

