#pragma once

#include "types.h"
#include "irq_chip.h"
#include "l4_types.h"
#include <cxx/type_traits>

/**
 * Interface used to manage hardware IRQs on a platform.
 *
 * The main purpose of this interface is to allow an
 * abstract mapping of global IRQ numbers to a chip
 * and pin number pair. The interface provides also
 * some global information about IRQs.
 */
class Irq_mgr
{
public:
  using Msi_info = Irq_chip_icu_msi::Msi_info;

  /**
   * Chip and pin for an IRQ pin.
   */
  struct Irq
  {
    // allow uninitialized instances
    enum Init { Bss };
    Irq(Init) {}

    /// Invalid IRQ.
    Irq() : chip(0) {}

    /// Create a chip-pin pair.
    Irq(Irq_chip_icu *chip, Mword pin) : chip(chip), pin(pin) {}

    /// The chip.
    Irq_chip_icu *chip;

    /// The pin number local to \a chip.
    Mword pin;

    Irq_base *irq() const
    { return chip->irq(pin); }
  };

  /// Map legacy (IA32) IRQ numbers to valid IRQ numbers.
  virtual unsigned legacy_override(Mword irqnum) { return irqnum; }

  /// Get the chip-pin pair for the given global IRQ number.
  virtual Irq chip(Mword irqnum) const = 0;

  /// Get the highest available global IRQ number plus 1.
  virtual unsigned nr_irqs() const = 0;

  /// Get the number of available entry points for MSIs.
  virtual unsigned nr_msis() const = 0;

  /** Get the message to use for a given MSI.
   * \pre The IRQ pin needs to be already allocated before using this function.
   */
  virtual int msg(Mword /* irqnum */, Unsigned64, Msi_info *) const
  { return -L4_err::ENosys; }

  virtual void set_cpu(Mword irqnum, Cpu_number cpu) const
  {
    Irq i = chip(irqnum);
    if (!i.chip)
      return;

    return i.chip->set_cpu(i.pin, cpu);
  }

  /// The pointer to the single global instance of the actual IRQ manager.
  static Irq_mgr *mgr;

  /// Prevent generation of a real virtual delete function
  virtual ~Irq_mgr() = 0;

  bool alloc(Irq_base *irq, Mword global_irq, bool init = true)
  {
    Irq i = chip(global_irq);
    if (!i.chip)
      return false;

    if (!i.chip->alloc(irq, i.pin, init))
      return false;

    if (init)
      i.chip->set_cpu(i.pin, Cpu_number::boot_cpu());

    return true;
  }

  bool reserve(Mword irqnum)
  {
    Irq i = chip(irqnum);
    if (!i.chip)
      return false;

    return i.chip->reserve(i.pin);
  }

  Irq_base *irq(Mword irqnum) const
  {
    Irq i = chip(irqnum);
    if (!i.chip)
      return 0;

    return i.chip->irq(i.pin);
  }

  virtual void init_ap(Cpu_number cpu, bool resume) = 0;
};

class Irq_mgr_dyn : public Irq_mgr
{
public:
  enum Errors : int
  {
    E_unaligned_base = 1,
    E_range = 2,
    E_too_many_chips = 3,
    E_zero_pins = 4,
    E_irqs_in_use = 5,
    E_no_chip = 6,
    E_ok = 0,
  };

  virtual int add_chip(int base, Irq_chip_icu *chip, int pins = -1) = 0;
  virtual int add_msi_chip(Irq_chip_icu_msi *chip) = 0;
};

template< typename CHIP >
class Irq_mgr_single_chip : public Irq_mgr
{
public:
  Irq_mgr_single_chip() {}

  template< typename... A >
  explicit Irq_mgr_single_chip(A&&... args) : c(cxx::forward<A>(args)...) {}

  Irq chip(Mword irqnum) const override { return Irq(&c, irqnum); }
  unsigned nr_irqs() const override { return c.nr_irqs(); }
  unsigned nr_msis() const override { return 0; }
  mutable CHIP c;
  void init_ap(Cpu_number cpu, bool resume) override { c.init_ap(cpu, resume); }
};

inline Irq_mgr::~Irq_mgr() {}

