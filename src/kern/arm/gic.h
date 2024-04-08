
#pragma once

#include "cascade_irq.h"
#include "cpu.h"
#include "irq_chip_generic.h"
#include "gic_dist.h"
#include "globalconfig.h"

#include <cassert>
#include <cstdio>

class Gic : public Irq_chip_gen
{
  friend class Jdb;

protected:
  Gic_dist _dist;

public:
  static void set_irq_handler(void (*irq_handler)());

  explicit Gic(Address dist_base) : _dist(dist_base) {}

  virtual void softint_cpu(Cpu_number target, unsigned m) = 0;


  // init / pm only functions (rarely used)
  virtual void softint_bcast(unsigned m) = 0;
  virtual void softint_phys(unsigned m, Unsigned64 target) = 0;
  virtual void init_ap(Cpu_number cpu, bool resume) = 0;
  virtual unsigned gic_version() const = 0;

  // empty default for JDB
  virtual void irq_prio_bootcpu(unsigned, unsigned) {}
  virtual unsigned irq_prio_bootcpu(unsigned) { return 0; }
  virtual unsigned get_pmr() { return 0; }
  virtual void set_pmr(unsigned) {}
  virtual unsigned get_pending() { return 1023; }

  unsigned hw_nr_irqs()
  { return _dist.hw_nr_irqs(); }

  void disable_locked(unsigned irq)
  { _dist.disable_irq(irq); }

  void enable_locked(unsigned irq)
  { _dist.enable_irq(irq); }

  void set_pending_irq(unsigned idx, Unsigned32 val)
  {
    _dist.set_pending_irq(idx, val);
  }

  void mask(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
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

#if defined (CONFIG_JDB)
  char const *chip_type() const override
  { return "GIC"; }
#endif // CONFIG_JDB

};

template<typename IMPL, typename CPU>
class Gic_mixin : public Gic
{
private:
  friend class Jdb;

  using Self = IMPL;
  IMPL const *self() const { return static_cast<IMPL const *>(this); }
  IMPL *self() { return static_cast<IMPL *>(this); }

  using Cpu = CPU;

protected:
  Cpu _cpu;

  static IMPL *primary;

  static void _glbl_irq_handler()
  { primary->hit(nullptr); }

  void init_global_irq_handler()
  {
    primary = self();
    Gic::set_irq_handler(_glbl_irq_handler);
  }

public:
  template<typename ...CPU_ARGS>
  Gic_mixin(Address dist_base, int nr_irqs_override, CPU_ARGS &&...args)
  : Gic(dist_base), _cpu(cxx::forward<CPU_ARGS>(args)...)
  {
    unsigned num = init(true, nr_irqs_override);
    printf("Number of IRQs available at this GIC: %d\n", num);
    Irq_chip_gen::init(num);
  }

  /**
   * \brief Create a GIC device that is a physical alias for the
   *        master GIC.
   */
  template<typename ...CPU_ARGS>
  Gic_mixin(Address dist_base, Gic *master_mapping, CPU_ARGS &&...args)
  : Gic(dist_base), _cpu(cxx::forward<CPU_ARGS>(args)...)
  {
    Irq_chip_gen::init(master_mapping->nr_irqs());
  }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    _cpu.disable();

    if (!resume)
      self()->cpu_local_init(cpu);

    _cpu.enable();
  }

  unsigned init(bool primary_gic, int nr_irqs_override = -1)
  {
    if (!primary_gic)
      {
        self()->cpu_local_init(Cpu_number::boot_cpu());
        return 0;
      }

    _cpu.disable();
    unsigned num = _dist.init(typename IMPL::Version(),
                              Cpu::Cpu_prio_val, nr_irqs_override);

    self()->init_global_irq_handler();

    return num;
  }

  void acknowledge_locked(unsigned irq)
  {
    if (!Gic_dist::Config_mxc_tzic)
      _cpu.ack(irq);
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
    if (Gic_dist::Config_mxc_tzic)
      return _dist.mxc_pending();

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

    handle_irq<Gic>(num, u);
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
};

template<typename IMPL, typename CPU>
IMPL *Gic_mixin<IMPL, CPU>::primary;


