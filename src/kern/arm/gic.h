
#pragma once

#include <cascade_irq.h>
#include <cpu.h>
#include <irq_chip_generic.h>
#include <gic_dist.h>
#include <gic_iface.h>
#include <irq_entry.h>
#include <globalconfig.h>

#include <cassert>
#include <cstdio>

class Gic_x : public Gic
{
  friend class Jdb;

protected:
  Gic_dist _dist;

public:
  IRQ_CHIP_DBG_INFO("GIC");

  explicit Gic_x(Address dist_base) : _dist(dist_base) {}

  Address get_dist_base() const
  {
    return _dist.get_mmio_base();
  }

  unsigned hw_nr_irqs()
  { return _dist.hw_nr_irqs(); }

  void enable_locked(unsigned irq)
  { _dist.enable_irq(irq); }

  void set_pending_irq(unsigned idx, Unsigned32 val) override
  {
    _dist.set_pending_irq(idx, val);
  }

  void unmask(Mword pin) override
  {
    assert (cpu_lock.test());
    enable_locked(pin);
  }

  int set_mode(Mword pin, Mode m) override
  {
    return _dist.set_mode(pin, m);
  }

  bool is_edge_triggered(Mword pin) const override
  {
    return _dist.is_edge_triggered(pin);
  }

  bool alloc(Irq_base *irq, Mword pin, bool init = true) override;
};

template<typename IMPL, typename CPU>
class Gic_mixin : public Gic_x
{
private:
  friend class Jdb;

  using Self = IMPL;
  IMPL const *self() const { return static_cast<IMPL const *>(this); }
  IMPL *self() { return static_cast<IMPL *>(this); }

  using Cpu = CPU;

protected:
  Cpu _cpu;

  static void _glbl_irq_handler()
  {
    nonull_static_cast<IMPL *>(primary)->hit(nullptr);
  }

public:
  template<typename ...CPU_ARGS>
  Gic_mixin(Address dist_base, CPU_ARGS &&...args)
  : Gic_x(dist_base), _cpu(cxx::forward<CPU_ARGS>(args)...)
  {}

  void set_as_primary_irq_handler()
  {
    primary = self();
    Arm_irqs::set_irq_handler(_glbl_irq_handler);
  }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    _cpu.disable();

    if (!resume)
      self()->cpu_local_init(cpu);

    _cpu.enable();
  }

  void cpu_deinit(Cpu_number cpu) override
  {
    self()->migrate_irqs(cpu, Cpu_number::boot_cpu());
    self()->redist_disable(cpu);
    _cpu.disable();
  }

  void acknowledge_locked(unsigned irq)
  {
    _cpu.ack(irq);
  }

  void mask(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
  }

  void mask_and_ack(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
    acknowledge_locked(pin);
  }

  void ack(Mword pin) override
  {
    acknowledge_locked(pin);
  }

  unsigned gic_version() const override
  { return IMPL::Version::value; }

  Unsigned32 pending()
  {
    Unsigned32 ack = _cpu.iar();

    // GICv2 only: for SGIs, bits [12:10] identify the source CPU interface.
    // For all other interrupts these bits are zero.
    Unsigned32 intid = ack & Cpu::Cpu_iar_intid_mask;

    // Ack SGIs (IPIs) immediately, the whole ack value must be used,
    // including the source CPU interface identifier.
    if (intid < 16)
      _cpu.ack(ack);

    return intid;
  }

  unsigned get_pending() override
  { return pending(); }

  void hit(Upstream_irq const *u)
  {
    Unsigned32 num = pending();

    // INTIDs 1020 - 1023 are spurious on GIC v2 and v3 and do not need an EOI
    if (EXPECT_FALSE((num & 0xfffffffc) == 0x3fc))
      return;

    handle_irq<Gic_x>(num, u);
  }

  static void cascade_hit(Irq_base *_self, Upstream_irq const *u)
  {
    // this function calls some virtual functions that might be
    // ironed out
    Cascade_irq *self = nonull_static_cast<Cascade_irq*>(_self);
    Self *gic = nonull_static_cast<Self*>(self->child());
    Upstream_irq ui(self, u);
    gic->hit(&ui);
  }

  unsigned get_pmr() override { return _cpu.pmr(); }
  void set_pmr(unsigned prio) override { _cpu.pmr(prio); }
  void disable_locked(unsigned irq)
  { _dist.disable_irq(typename IMPL::Version(), irq); }

protected:
  unsigned init_dist(int nr_irqs_override = -1)
  {
    _cpu.disable();
    unsigned num = _dist.init(typename IMPL::Version(),
                              Cpu::Cpu_prio_val, nr_irqs_override);
    return num;
  }
};

