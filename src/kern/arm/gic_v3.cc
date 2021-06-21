
#include "gic_v3.h"
#include "globalconfig.h"


DEFINE_PER_CPU Per_cpu<Gic_redist> Gic_v3::_redist;

void
Gic_v3::set_cpu(Mword pin, Cpu_number cpu)
{
  _dist.set_cpu(pin, ::Cpu::cpus.cpu(cpu).phys_id(), Version());
}

void
Gic_v3::mask_percpu(Cpu_number cpu, Mword pin)
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).mask(pin);
}

void
Gic_v3::unmask_percpu(Cpu_number cpu, Mword pin)
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).unmask(pin);
}

int
Gic_v3::set_mode_percpu(Cpu_number cpu, Mword pin, Mode m)
{
  assert(pin < 32);
  assert (cpu_lock.test());
  return _redist.cpu(cpu).set_mode(pin, m);
}

//-------------------------------------------------------------------
#if defined (CONFIG_JDB)

void
Gic_v3::irq_prio_bootcpu(unsigned irq, unsigned prio)
{
  assert(irq < 32);
  _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq, prio);
}

unsigned
Gic_v3::irq_prio_bootcpu(unsigned irq)
{
  assert(irq < 32);
  return _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq);
}

#endif // CONFIG_JDB


#ifdef CONFIG_ARM_GIC_MSI

#include <gic_msi.h>
#include <gic_its.h>

void
Gic_v3::init_lpi(Address its_base)
{
  unsigned hw_num_lpis = _dist.hw_nr_lpis();
  _has_lpis = hw_num_lpis > 0;
  if (_has_lpis)
    {
      unsigned num_lpis = min<unsigned>(hw_num_lpis, Max_num_lpis);

      Gic_redist::init_lpi(num_lpis);
      _its = new Boot_object<Gic_its>();
      _its->init(&_cpu, its_base, num_lpis);
      _msi = new Boot_object<Gic_msi>();
      _msi->init(_its, num_lpis);
      printf("GIC: Supports up to %u LPIs, using %u.\n", hw_num_lpis, num_lpis);
    }
  else
    WARN("GIC: Does not implement LPIs...\n");
}

void
Gic_v3::cpu_local_init_lpi(Cpu_number cpu)
{
  if (_has_lpis)
  {
    _redist.cpu(cpu).cpu_init_lpi();
    _its->cpu_init(cpu, _redist.cpu(cpu));
  }
}

Irq_base *
Gic_v3::irq(Mword pin) const
{
  if (_has_lpis && pin >= Gic_dist::Lpi_intid_base)
    return _msi->Gic_msi::irq(pin - Gic_dist::Lpi_intid_base);

  return this->Gic::irq(pin);
}

#endif
