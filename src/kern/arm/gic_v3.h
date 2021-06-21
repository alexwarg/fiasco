#pragma once

#include "gic.h"
#include "gic_redist.h"
#include "gic_cpu_v3.h"
#include "globalconfig.h"

class Gic_msi;
class Gic_its;

class Gic_v3 : public Gic_mixin<Gic_v3, Gic_cpu_v3>
{
  using Gic = Gic_mixin<Gic_v3, Gic_cpu_v3>;

  static Per_cpu<Gic_redist> _redist;
  Per_cpu_array<Unsigned64> _sgi_template;

  Address _redist_base;

public:
  using Version = Gic_dist::V3;

  explicit Gic_v3(Address dist_base, Address redist_base)
  : Gic(dist_base), _redist_base(redist_base)
  {
    init_gic(-1);
  }

  void init_gic(int nr_irqs_override = -1)
  {
    unsigned num = init_dist(nr_irqs_override);
    printf("Number of IRQs available at this GIC: %d\n", num);
    Irq_chip_gen::init(num);

    cpu_local_init(Cpu_number::boot_cpu());
    _cpu.enable();
  }

  void set_cpu(Mword pin, Cpu_number cpu) override;
  void mask_percpu(Cpu_number cpu, Mword pin) override;
  void unmask_percpu(Cpu_number cpu, Mword pin) override;
  int set_mode_percpu(Cpu_number cpu, Mword pin, Mode m) override;

#if defined (CONFIG_JDB)
  void irq_prio_bootcpu(unsigned irq, unsigned prio) override;
  unsigned irq_prio_bootcpu(unsigned irq) override;
#endif // CONFIG_JDB

  void softint_cpu(Cpu_number target, unsigned m) override
  {
    Unsigned64 sgi = _sgi_template[target] | (m << 24);
    _cpu.softint(sgi);
  }

  void softint_bcast(unsigned m) override
  { _cpu.softint((1ull << 40) | (m << 24)); }

  void softint_phys(unsigned m, Unsigned64 target) override
  { _cpu.softint(target | (m << 24)); }

  void cpu_local_init(Cpu_number cpu)
  {
    auto &rd = _redist.cpu(cpu);
    Unsigned64 mpidr = ::Cpu::mpidr();

    rd.find(_redist_base, mpidr, cpu);
    rd.cpu_init();

    if (mpidr & 0xf0)
      {
        _sgi_template[cpu] = ~0ull;
        printf("GICv3: Cpu%u affinity level 0 out of range: %u max is 15\n",
               cxx::int_value<Cpu_number>(cpu), (unsigned)(mpidr & 0xff));
        return;
      }

    _sgi_template[cpu] = (1u << (mpidr & 0xf))
                         | ((mpidr & (Unsigned64)0xff00) << 8)
                         | ((mpidr & (Unsigned64)0xff00ff0000) << 16);
  }

#ifdef CONFIG_ARM_GIC_MSI
private:
  enum
  {
    // Limit the number of supported LPIs to avoid excessive memory
    // allocations.
    Max_num_lpis = 512,
  };

  bool _has_lpis = false;
  Gic_its *_its = nullptr;
  Gic_msi *_msi = nullptr;

  void init_lpi(Address its_base);
  void cpu_local_init_lpi(Cpu_number cpu);

public:
  /**
   * \return The MSI Irq_chip for this GIC, might be nullptr if the GIC does not
   *         support LPIs or MSI support has been disabled in the Kconfig.
   */
  Gic_msi *msi_chip() const { return _msi; }
  Irq_base *irq(Mword pin) const override;

#else
public:
  Gic_msi *msi_chip() const { return _msi; }

#endif
};
