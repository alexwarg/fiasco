
#pragma once

#include <gic_its.h>
#include <irq_chip.h>
#include <globalconfig.h>
#include "cxx/static_vector"

class Gic_v3;

class Gic_msi : public Irq_chip_icu_msi
{
private:
  struct Msi_src
  {
    Unsigned64 raw;
    explicit Msi_src(Unsigned64 v) : raw(v) {}

    CXX_BITFIELD_MEMBER_RO( 0, 31, device_id, raw);
    /**
     * Number of the ITS that is responsible for the device to which the MSI
     * shall be bound. Kernel and user space must agree on the numbering of
     * the ITSs in the system. How this agreement is reached is platform
     * specific. For example, it may correspond to the order of the ITSs in
     * the MADT ACPI table.
     */
    CXX_BITFIELD_MEMBER_RO(32, 39, its_num, raw);
  };

  /**
   * Contains the state of an LPI and provides state transition methods.
   *
   * All methods in Lpi, except the constructor, assume that Lpi's lock is held.
   */
  class Lpi : public Gic_its::Lpi
  {
  public:
    Lpi() { reset(); }

    void alloc(Irq_base *irq, bool &success)
    {
      // Already allocated to different Irq object
      if (_irq)
        return;

      _irq = irq;
      success = true;
    }

    void free()
    {
      if (_its)
        _its->free_lpi(*this);
      reset();
    }

    void ack()
    {
      if (_its)
        _its->ack_lpi(*this);
    }

    void mask()
    {
      _enabled = false;
      if (_its)
        _its->mask_lpi(*this);
    }

    void mask_and_ack()
    {
      mask();
      ack();
    }

    void unmask()
    {
      _enabled = true;
      if (_its)
        _its->unmask_lpi(*this);
    }

    void set_cpu(Cpu_number cpu)
    {
      _cpu = cpu;
      if (_its)
        _its->assign_lpi_to_cpu(*this, _cpu);
    }

    void bind_to_device(Gic_its *its, Unsigned32 src, Irq_mgr::Msi_info *inf,
                        int &err)
    {
      if (_its != its)
      {
        // LPI is currently bound to a different ITS
        if (_its)
          _its->free_lpi(*this);

        // Assign LPI to new ITS.
        _its = its;

        // Transfer state from previously assigned ITS.
        if (_enabled)
          _its->unmask_lpi(*this);
        else
          _its->mask_lpi(*this);

        if (_cpu != Cpu_number::nil())
          _its->assign_lpi_to_cpu(*this, _cpu);
      }

      err = _its->bind_lpi_to_device(*this, src, inf);
    }

    Irq_base *irq() const { return _irq; }

    Cpu_number cpu() const { return _cpu; }

  private:
    void reset()
    {
      _irq = nullptr;
      _cpu = Cpu_number::nil();
      _enabled = false;
      _its = nullptr;
    }

    Irq_base *_irq;
    Cpu_number _cpu;
    bool _enabled;
    Gic_its *_its;
  };

  template<typename CB, typename... ARGS>
  bool with_lpi(Mword pin, CB &&cb, ARGS &&...args)
  {
    if (pin >= _lpis.size())
      return false;

    Lpi &lpi = _lpis[pin];
    auto g = lock_guard(lpi.lock);
    (lpi.*cb)(cxx::forward<ARGS>(args)...);
    return true;
  }

  Gic_v3 *_gic;
  using Lpi_vec = cxx::static_vector<Lpi>;
  Lpi_vec _lpis;

public:
  Gic_msi(Gic_v3 *gic, unsigned num_lpis);

  unsigned nr_irqs() const override
  { return _lpis.size(); }

  Irq_base *irq(Mword pin) const override;
  bool alloc(Irq_base *irq, Mword pin, bool init = true) override
  {
    bool success = false;
    with_lpi(pin, &Lpi::alloc, irq, success);

    if (success)
      bind(irq, pin, !init);

    return success;
  }

  int set_mode(Mword, Mode) override
  { return 0; }

  bool is_edge_triggered(Mword) const override
  { return true; }

  void mask(Mword pin) override
  { with_lpi(pin, &Lpi::mask); }

  void ack(Mword pin) override
  { with_lpi(pin, &Lpi::ack); }

  void mask_and_ack(Mword pin) override
  { with_lpi(pin, &Lpi::mask_and_ack); }

  void unmask(Mword pin) override
  { with_lpi(pin, &Lpi::unmask); }

  void set_cpu(Mword pin, Cpu_number cpu) override
  { with_lpi(pin, &Lpi::set_cpu, cpu); }

  void unbind(Irq_base *irq) override
  {
    with_lpi(irq->pin(), &Lpi::free);
    Irq_chip_icu::unbind(irq);
  }

  bool reserve(Mword pin) override
  {
    bool success = false;
    with_lpi(pin, &Lpi::alloc, (Irq_base*)1, success);
    return success;
  }

  int msg(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *inf) override;
  void migrate_lpis(Cpu_number from, Cpu_number to);

#ifdef CONFIG_JDB
  char const *chip_type() const override
  { return "GIC-MSI"; }
#endif
};


