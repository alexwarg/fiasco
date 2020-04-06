#pragma once

#include <gic.h>
#include <gic_v2.h>

struct Ext_gic : Gic
{
  IRQ_CHIP_DBG_INFO("Ext GIC");

  unsigned long offset = 0;
  Per_cpu_array<Static_object<Gic_v2>> g;
  Gic_v2 *current() { return g[current_cpu()]; }
  Gic_v2 *master() { return g[Cpu_number::boot_cpu()]; }
  Gic_v2 const *current() const { return g[current_cpu()]; }

  Ext_gic(Address cpu_base, Address dist_base, unsigned long offset, int nr_irqs_override = -1)
  : offset(offset)
  {
    g[Cpu_number::boot_cpu()].construct(cpu_base, dist_base, nullptr);
    master()->init_gic(nr_irqs_override);
  }

  void ack(Mword pin) override
  { current()->Gic_v2::ack(pin); }

  void mask_and_ack(Mword pin) override
  { current()->Gic_v2::mask_and_ack(pin); }

  void set_cpu(Mword pin, Cpu_number cpu) override
  { g[cpu]->Gic_v2::set_cpu(pin, cpu); }

  void softint_cpu(Cpu_number target, unsigned m) override
  { current()->Gic_v2::softint_cpu(target, m); }

  void softint_bcast(unsigned m) override
  { current()->Gic_v2::softint_bcast(m); }

  void softint_phys(unsigned m, Unsigned64 target) override
  { current()->Gic_v2::softint_phys(m, target); }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    if (!resume)
      {
        unsigned phys_cpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());
        g[cpu].construct(
            master()->get_dist_base() + phys_cpu * offset,
            master()->get_cpu_base()  + phys_cpu * offset,
            master());
      }
    g[cpu]->Gic_v2::init_ap(cpu, resume);
  }

  void cpu_deinit(Cpu_number) override
  {}

  unsigned gic_version() const override
  { return 2; }

  void hit(Upstream_irq const *ui)
  {
    Unsigned32 num = current()->pending();

    // INTIDs 1020 - 1023 are spurious on GIC v2 and v3 and do not need an EOI
    if (EXPECT_FALSE((num & 0xfffffffc) == 0x3fc))
      return;

    handle_irq<Ext_gic>(num, ui);
  }

  void disable_locked(unsigned irq)
  { current()->Gic_v2::disable_locked(irq); }

  void enable_locked(unsigned irq)
  { current()->Gic_v2::enable_locked(irq); }

  void set_pending_irq(unsigned idx, Unsigned32 val)
  {
    current()->Gic_v2::set_pending_irq(idx, val);
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
    return current()->Gic_v2::set_mode(pin, m);
  }

  bool is_edge_triggered(Mword pin) const override
  {
    return current()->Gic_v2::is_edge_triggered(pin);
  }

};


