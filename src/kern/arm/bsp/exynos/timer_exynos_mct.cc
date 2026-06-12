#include <timer.h>

#include <cpu.h>
#include <io.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <cstdio>

Static_object<Mct_timer> Timer::mct;
DEFINE_PER_CPU Per_cpu<Static_object<Timer> > Timer::timers;

void
Timer::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      mct.construct(Kmem_mmio::map(Mem_layout::Mct_phys_base, 0x100));
      mct->write<Mword>(0, Mct_timer::Reg::Cfg);
    }

  Address timer_addr = Mem_layout::Mct_phys_base + 0x300
                     + cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id()) * 0x100;
  timers.cpu(cpu).construct(Kmem_mmio::map(timer_addr, 0x100));
  timers.cpu(cpu)->Mct_core_timer::configure();
}

