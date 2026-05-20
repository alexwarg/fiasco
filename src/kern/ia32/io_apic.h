#pragma once

#include <types.h>
#include <spin_lock.h>
#include <lock_guard.h>
#include <irq_chip_ia32.h>
#include <cxx/bitfield>
#include <irq_mgr.h>
#include <pm.h>
#include <globalconfig.h>

#include <cassert>

class Acpi_madt;

class Io_apic_entry
{
  friend class Io_apic;
private:
  Unsigned64 _e;

public:
  enum Delivery { Fixed, Lowest_prio, SMI, NMI = 4, INIT, ExtINT = 7 };
  enum Dest_mode { Physical, Logical };
  enum Polarity { High_active, Low_active };
  enum Trigger { Edge, Level };

  Io_apic_entry() = default;
  Io_apic_entry(Unsigned8 vector, Delivery d, Dest_mode dm, Polarity p,
                Trigger t, Unsigned32 dest)
    : _e(  vector_bfm_t::val(vector) | delivery_bfm_t::val(d) | mask_bfm_t::val(1)
         | dest_mode_bfm_t::val(dm)  | polarity_bfm_t::val(p)
         | trigger_bfm_t::val(t)     | dest_bfm_t::val(dest >> 24))
  {}

  CXX_BITFIELD_MEMBER( 0,  7, vector, _e);
  CXX_BITFIELD_MEMBER( 8, 10, delivery, _e);
  CXX_BITFIELD_MEMBER(11, 11, dest_mode, _e);
  CXX_BITFIELD_MEMBER(13, 13, polarity, _e);
  CXX_BITFIELD_MEMBER(15, 15, trigger, _e);
  CXX_BITFIELD_MEMBER(16, 16, mask, _e);
  // support for IRQ remapping
  CXX_BITFIELD_MEMBER(48, 48, format, _e);
  // support for IRQ remapping
  CXX_BITFIELD_MEMBER(49, 63, irt_index, _e);
  CXX_BITFIELD_MEMBER(56, 63, dest, _e);
};


class Io_apic : public Irq_chip_icu, protected Irq_chip_ia32
{
  friend class Jdb_io_apic_module;
  friend class Irq_chip_ia32;
public:
  IRQ_CHIP_DBG_INFO("IO-APIC");

  explicit Io_apic(Unsigned64 phys, unsigned gsi_base);

  unsigned nr_irqs() const override { return Irq_chip_ia32::nr_irqs(); }
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

  void mask(Mword irq) override;
  void ack(Mword) override;
  void mask_and_ack(Mword irq) override;
  void unmask(Mword irq) override;
  void set_cpu(Mword irq, Cpu_number cpu) override;
  int set_mode(Mword pin, Mode mode) override;
  bool is_edge_triggered(Mword pin) const override;
  bool alloc(Irq_base *irq, Mword pin, bool init = true) override;
  void unbind(Irq_base *irq) override;

  void dump();

  bool valid() const
  {
    return _apic;
  }

  Io_apic_entry read_entry(unsigned i) const
  {
    auto g = lock_guard(_l);
    Io_apic_entry e;
    //assert(i <= num_entries());
    e._e = (Unsigned64)_apic->read(0x10+2*i) | (((Unsigned64)_apic->read(0x11+2*i)) << 32);
    return e;
  }

  void write_entry(unsigned i, Io_apic_entry const &e)
  {
    auto g = lock_guard(_l);
    //assert(i <= num_entries());
    _apic->write(0x10+2*i, e._e);
    _apic->write(0x11+2*i, e._e >> 32);
  }

  bool masked(unsigned irq)
  {
    auto g = lock_guard(_l);
    //assert(irq <= _apic->num_entries());
    return _apic->read(0x10 + irq * 2) & (1UL << 16);
  }

  void sync()
  {
    (void)_apic->data;
  }

  void set_dest(unsigned irq, Mword dst)
  {
    auto g = lock_guard(_l);
    //assert(irq <= _apic->num_entries());
    _apic->modify(0x11 + irq * 2, dst & (~0UL << 24), ~0UL << 24);
  }

  unsigned gsi_offset() const
  {
    return _offset;
  }

  Irq_chip::Mode get_mode(Mword pin)
  {
    Io_apic_entry e = read_entry(pin);
    Mode m(Mode::Set_irq_mode);
    m.polarity() = e.polarity() == Io_apic_entry::High_active
                 ? Mode::Polarity_high
                 : Mode::Polarity_low;
    m.level_triggered() = e.trigger() == Io_apic_entry::Level
                        ? Mode::Trigger_level
                        : Mode::Trigger_edge;
    return m;
  }

  static void init(Cpu_number);
  static bool init_scan_apics();
  static Acpi_madt const *lookup_madt();

  static Acpi_madt const *madt()
  {
    return _madt;
  }

  static void save_state();
  static void restore_state(bool set_boot_cpu = false);

  static unsigned total_irqs();

  static unsigned legacy_override(unsigned i);
  static Io_apic *find_apic(unsigned irqnum);
  struct iterator
  {
    Io_apic *c = nullptr;
    Io_apic *operator -> () const noexcept { return c; }
    Io_apic *operator * () const noexcept { return c; }
    iterator operator ++ (int) noexcept { iterator res = *this; c = c->_next; return res; }
    iterator operator ++ () noexcept { c = c->_next; return *this; }
    bool operator == (iterator const &o) const noexcept { return c == o.c; }
    bool operator != (iterator const &o) const noexcept { return c != o.c; }
  };

  struct it_range
  {
    iterator begin() noexcept { return iterator{Io_apic::_first}; }
    iterator end() noexcept { return iterator{nullptr}; }
  };

  static it_range all() { return it_range{}; }

  static bool active()
  {
    return _first;
  }

protected:
  static void read_overrides();

  static Mword to_io_apic_trigger(Irq_chip::Mode mode)
  {
    return mode.level_triggered()
           ? Io_apic_entry::Level
           : Io_apic_entry::Edge;
  }

  static Mword to_io_apic_polarity(Irq_chip::Mode mode)
  {
    return mode.polarity() == Irq_chip::Mode::Polarity_high
           ? Io_apic_entry::High_active
           : Io_apic_entry::Low_active;
  }

private:
  struct Apic
  {
    Unsigned32 volatile adr;
    Unsigned32 dummy[3];
    Unsigned32 volatile data;

    unsigned num_entries()
    {
      return (read(1) >> 16) & 0xff;
    }

    Mword read(int reg)
    {
      adr = reg;
      asm volatile ("": : :"memory");
      return data;
    }

    void modify(int reg, Mword set_bits, Mword del_bits)
    {
      Mword tmp;
      adr = reg;
      asm volatile ("": : :"memory");
      tmp = data;
      tmp &= ~del_bits;
      tmp |= set_bits;
      data = tmp;
    }

    void write(int reg, Mword value)
    {
      adr = reg;
      asm volatile ("": : :"memory");
      data = value;
    }

  } __attribute__((packed));

  Apic *_apic;
  mutable Spin_lock<> _l;
  unsigned _offset;
  Io_apic *_next;

  static unsigned _nr_irqs;
  static Io_apic *_first;
  static Acpi_madt const *_madt;
  static Io_apic_entry *_state_save_area;

  void _mask(unsigned irq)
  {
    auto g = lock_guard(_l);
    //assert(irq <= _apic->num_entries());
    _apic->modify(0x10 + irq * 2, 1UL << 16, 0);
  }

  void _unmask(unsigned irq)
  {
    auto g = lock_guard(_l);
    //assert(irq <= _apic->num_entries());
    _apic->modify(0x10 + irq * 2, 0, 1UL << 16);
  }

};

class Io_apic_mgr : public Irq_mgr, public Pm_object
{
public:
  Io_apic_mgr() { register_pm(Cpu_number::boot_cpu()); }
  Irq chip(Mword irq) const override;
  unsigned nr_irqs() const override;
  unsigned nr_msis() const override;
  unsigned legacy_override(Mword i) override;

  void pm_on_suspend(Cpu_number cpu) override;
  void pm_on_resume(Cpu_number cpu) override;
  void init_ap(Cpu_number cpu, bool resume) override;
};

