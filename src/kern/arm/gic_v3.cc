
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

