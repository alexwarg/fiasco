
#include <gic.h>

#include <cassert>
#include <cstdio>
#include <string.h>
#include <globalconfig.h>

#ifdef CONFIG_MP

#include "cpu.h"
#include "ipi.h"
#include "mips_cpu_irqs.h"
#include "processor.h"

void
Gic::set_cpu(Mword pin, Cpu_number cpu)
{
  auto pcpu = Cpu::cpus.cpu(cpu).phys_id();
  // AW11: avoid setting two bits in two different words of the
  // GIC_SH_MAPi_COREn registers. So limit the maximum phys CPU number
  // to 31;
  assert (cxx::int_value<Cpu_phys_id>(pcpu) < 32);

  _r[sh_map_core(pin, pcpu)] = sh_map_core_bit(pcpu);
}

void
Gic::send_ipi(Cpu_number to, Ipi *)
{
  unsigned irq = _ipi_base + cxx::int_value<Cpu_number>(to);
  _r[Sh_wedge] = (1UL << 31) | irq;
}

void
Gic::ack_ipi(Cpu_number cpu)
{
  asm volatile ("sync" : : : "memory");
  unsigned irq = _ipi_base + cxx::int_value<Cpu_number>(cpu);
  _r[Sh_wedge] = irq;
}

void
Gic::init_ipis(Cpu_number cpu, Irq_base *irq)
{
  if (cpu == Cpu_number::boot_cpu())
    check(Mips_cpu_irqs::chip->alloc(irq, _cpu_int_ipi));
  else
    Mips_cpu_irqs::chip->unmask(_cpu_int_ipi);

  unsigned i = _ipi_base + cxx::int_value<Cpu_number>(cpu);
  auto cpuid = Proc::cpu_id();
  _r[sh_map_core(i, cpuid)] = sh_map_core_bit(cpuid);
}

inline void
Gic::setup_ipis()
{
  /* make sure we have at least 16 (arbitrary) IRQs left after
   * assigning IPIs */
  assert (Config::Max_num_cpus <= (nr_irqs() - 16));
  _ipi_base = nr_irqs() - Config::Max_num_cpus;
  printf("GIC: IPI base is: %u\n", _ipi_base);

  //FIXME: limit the number of user visible IRQs by the IPIs allocated
  for (unsigned i = _ipi_base; i < nr_irqs(); ++i)
    {
      _r.r<Unsigned32>(sh_map_pin(i)) = (1UL << 31) | (_cpu_int_ipi - 2);
      auto mask = sh_irq_bit(i);
      _r[sh_irq_reg(Sh_pol, i)].set(mask);   // pol high
      _r[sh_irq_reg(Sh_trigger, i)].set(mask);   // edge
      _r[sh_irq_reg(Sh_dual_edge, i)].clear(mask); // single edge
      asm volatile ("sync" : : : "memory");
      _r[Sh_wedge] = i;                   // clear IRQ
      asm volatile ("sync" : : : "memory");
      _r[sh_irq_reg(Sh_smask, i)] = mask;      // enable
    }

  Ipi::hw = this;
}

#else // CONFIG_MP

inline void Gic::setup_ipis() {}

void
Gic::set_cpu(Mword, Cpu_number)
{}

#endif // CONFIG_MP

Gic::Gic(void *mmio, unsigned cpu_int) : _r(mmio), _mode_lock(Spin_lock<>::Unlocked)
{
  Reg_type cfg = _r[Sh_config];
  unsigned vpes = (cfg & 0x3f) + 1;
  unsigned nrirqs = (((cfg >> 16) & 0xff) + 1) * 8;
  Reg_type rev = _r[Sh_revision_id];

  printf("MIPS GIC[%08lx]: %u IRQs %u VPEs%s, V%d.%d\n",
         reinterpret_cast<Address>(mmio), nrirqs, vpes, (cfg & (1 << 31)) ? "VZP" : "",
         (unsigned) rev >> 8, (unsigned) rev & 0xff);

  assert (vpes <= 32); // this limit is due to set_cpu limitations
  assert (nrirqs <= 256);

  assert (cpu_int >= Cpu_int_offset && cpu_int < 8);
  cpu_int -= Cpu_int_offset;

  init(nrirqs);

  memset(_enabled_map, 0, sizeof(_enabled_map));

  for (unsigned i = 0; i < (nrirqs + Reg_width - 1) / Reg_width; ++i)
    {
      _r[Sh_rmask   + i * Reg_bytes] = ~0UL; // disabled
      _r[Sh_pol     + i * Reg_bytes] = ~0UL; // pol high
      _r[Sh_trigger + i * Reg_bytes] = 0; // level
    }

  for (unsigned i = 0; i < nrirqs; ++i)
    {
      // default to core 0
      _r[sh_map_core(i, Cpu_phys_id(0))] = 1;
      for (unsigned id = Reg_width; id < 256; id += Reg_width)
        _r[sh_map_core(i, Cpu_phys_id(id))]  = 0;

      _r.r<Unsigned32>(sh_map_pin(i)) = (1UL << 31) | cpu_int;
    }

  setup_ipis();
}

int
Gic::set_mode(Mword pin, Mode mode)
{
  if (!mode.set_mode())
    return 0;

  auto smask = sh_irq_reg(Sh_smask, pin);
  auto rmask = sh_irq_reg(Sh_rmask, pin);
  auto pol = sh_irq_reg(Sh_pol, pin);
  auto trig = sh_irq_reg(Sh_trigger, pin);
  auto dual = sh_irq_reg(Sh_dual_edge, pin);
  auto bit = sh_irq_bit(pin);

  auto guard = lock_guard(_mode_lock);

  bool lvl = mode.level_triggered();
  bool pol_h = mode.polarity() == Mode::Polarity_high;

  auto mask = _r.read<Reg_type>(sh_irq_reg(Sh_mask, pin)) & bit;
  if (mask)
    _r[rmask] = bit;

  _r[trig].modify(lvl ? 0 : bit, lvl ? bit : 0);
  _r[pol].modify(pol_h ? bit : 0, pol_h ? 0 : bit);
  if (!lvl)
    {
      bool d = mode.polarity() == Mode::Polarity_both;
      _r[dual].modify(d ? bit : 0, d ? 0 : bit);
    }

  if (mask)
    _r[smask] = bit;

  return 0;
}


