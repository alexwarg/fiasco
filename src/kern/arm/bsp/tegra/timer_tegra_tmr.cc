
#include <timer_tegra_tmr.h>
#include <kmem_mmio.h>
#include <mem_layout.h>

Static_object<Mmio_register_block> Timer_tegra_tmr::_tmr;

void
Timer_tegra_tmr::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      _tmr.construct(Kmem_mmio::map(Mem_layout::Tmr_phys_base, 0x10));
      _tmr->write<Mword>(  (1 << 31) // enable
                         | (1 << 30) // periodic
                         | (Config::Scheduler_granularity & 0x1fffffff),
                         Reg::PTV);
    }
}

