
#include "gic_v3.h"
#include <gic_its.h>
#include "globalconfig.h"
#include <boot_alloc.h>


DEFINE_PER_CPU Per_cpu<Gic_redist> Gic_v3::_redist;
namespace
{
  class Gic_redist_find_array : public Gic_redist_find
  {
  public:
    Gic_redist_find_array() = default;
    explicit Gic_redist_find_array(void *redist_base)
    : _redist_base(reinterpret_cast<Address>(redist_base)) {}

    Mmio_register_block get_redist_mmio(Unsigned64 mpid) override
    { return scan_range(_redist_base, mpid); }

  private:
    Address _redist_base;
  };
}

Gic_v3::Gic_v3(void *dist_base, void *redist_base)
: Gic(dist_base),
  _redist_get(new Boot_object<Gic_redist_find_array>(redist_base))
{
  init_gic(-1);
}

void
Gic_v3::set_cpu(Mword pin, Cpu_number cpu)
{
  _dist.set_cpu(pin, _dist.cpu_to_irouter_entry(cpu), Version());
}

void
Gic_v3::migrate_irqs(Cpu_number from, Cpu_number to)
{
  unsigned num = hw_nr_irqs();
  Unsigned64 val_from = _dist.cpu_to_irouter_entry(from);

  for (unsigned i = 0; i < num; ++i)
    if (_dist.irouter(i) == val_from)
      set_cpu(i, to);

  migrate_lpis(from, to);
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

void
Gic_v3::init_lpi()
{
  unsigned hw_num_lpis = _dist.hw_nr_lpis();
  _has_lpis = hw_num_lpis > 0;
  if (_has_lpis)
    {
      unsigned num_lpis = min<unsigned>(hw_num_lpis, Max_num_lpis);

      Gic_redist::init_lpi(num_lpis);
      _its_vec = Its_vec(Boot_alloced::allocate<Gic_its *>(Max_num_its),
                         Max_num_its);
      _msi = new Boot_object<Gic_msi>(this, num_lpis);
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
    for (unsigned i = 0; i < _num_its; i++)
      _its_vec[i]->cpu_init(cpu, _redist.cpu(cpu));
  }
}

void
Gic_v3::migrate_lpis(Cpu_number from, Cpu_number to)
{
  if (_has_lpis)
    _msi->Gic_msi::migrate_lpis(from, to);
}

Irq_base *
Gic_v3::irq(Mword pin) const
{
  if (_has_lpis && pin >= Gic_dist::Lpi_intid_base)
    return _msi->Gic_msi::irq(pin - Gic_dist::Lpi_intid_base);

  return this->Gic::irq(pin);
}

bool
Gic_v3::add_its(void *its_base)
{
  if (!_has_lpis)
    return false;

  if (_num_its >= _its_vec.size())
  {
    WARN("Maximum number of ITS exceeded.");
    return false;
  }

  Gic_its *its = new Boot_object<Gic_its>();
  its->init(&_cpu, its_base, _msi->nr_irqs());
  its->cpu_init(Cpu_number::boot_cpu(), _redist.cpu(Cpu_number::boot_cpu()));
  _its_vec[_num_its++] = its;
  return true;
}

#else

bool
Gic_v3::add_its(void *its_base)
{
  if (_dist.hw_nr_lpis() > 0)
    Gic_its::disable(its_base);
  return true;
}

#endif
