#pragma once

#include "gic.h"
#include "gic_cpu_v2.h"
#include "globalconfig.h"

class Gic_v2 : public Gic_mixin<Gic_v2, Gic_cpu_v2>
{
  using Gic = Gic_mixin<Gic_v2, Gic_cpu_v2>;
  Per_cpu_array<Unsigned32> _sgi_template;

public:
  using Version = Gic_dist::V2;

  Gic_v2(void *cpu_base, void *dist_base, int num_override = -1)
  : Gic(dist_base, cpu_base)
  {
    init_gic(num_override);
  }

  Gic_v2(void *cpu_base, void *dist_base, void *)
  : Gic(dist_base, cpu_base)
  {}

  void *get_cpu_base() const
  {
    return _cpu.get_mmio_base();
  }

  void init_gic(int nr_irqs_override = -1)
  {
    unsigned num = init_dist(nr_irqs_override);
    printf("Number of IRQs available at this GIC: %d\n", num);
    Irq_chip_gen::init(num);
    cpu_local_init(Cpu_number::boot_cpu());
    _cpu.enable();
  }

  void softint_cpu(Cpu_number target, unsigned m) override
  { _dist.softint(_sgi_template[target] | m); }

  void softint_bcast(unsigned m) override
  { _dist.softint((1u << 24) | m); }

  void softint_phys(unsigned m, Unsigned64 target) override
  { _dist.softint(target | m); }

  void redist_disable(Cpu_number)
  {}

  void cpu_local_init(Cpu_number cpu)
  {
    _dist.cpu_init_v2();
    // initialize our CPU target using the target register from some
    // CPU local IRQ
    _sgi_template[cpu] = _dist.itarget(0) & 0x00ff0000;
  }

  void migrate_irqs(Cpu_number from, Cpu_number to)
  {
    unsigned num = hw_nr_irqs();
    Unsigned8 val_from = _sgi_template[from] >> 16;

    for (unsigned i = 0; i < num; i += 4)
      {
        Unsigned32 itarget = _dist.itarget(i);
        for (unsigned j = 0; j < 4; ++j, itarget >>= 8)
          if ((itarget & 0xff) == val_from)
            set_cpu(i + j, to);
      }
  }

  void set_cpu(Mword pin, Cpu_number cpu) override
  {
    _dist.set_cpu(pin, _sgi_template[cpu] >> 16, Version());
  }

  Hit_func get_cascade_hit() override
  {
    return &cascade_hit;
  }

#if defined (CONFIG_JDB)
  void irq_prio_bootcpu(unsigned irq, unsigned prio) override
  {
    _dist.irq_prio(irq, prio);
  }

  unsigned irq_prio_bootcpu(unsigned irq) override
  {
    return _dist.irq_prio(irq);
  }
#endif // CONFIG_JDB
};

