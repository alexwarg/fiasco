#include <timer_tick-exynos-mct.h>
#include <globalconfig.h>
#include <platform.h>
#include <irq_mgr.h>


DEFINE_PER_CPU Per_cpu<Static_object<Timer_tick_exynos_mct> > Timer_tick_exynos_mct::_timer_ticks;

void
Timer_tick_exynos_mct::setup(Cpu_number cpu)
{
  int irq = -1;
  bool r;
  unsigned pcpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());


  if (Platform::is_5250() || Platform::is_5410())
    {
      assert(cpu < Cpu_number(Platform::is_5410() ? 4 : 2));
      irq = 152 + pcpu;
    }
  else if (Platform::is_4412())
    {
      assert(cpu < Cpu_number(4));
      irq = 28;
    }
  else
    {
      assert(cpu < Cpu_number(2));

      if (Platform::gic_int())
        irq = pcpu == 0 ? 96 + 51 * 8 + 0 : 96 + 35 * 8 + 3;
      else
        irq = pcpu == 0 ? 74 : 80;
    }

  if (irq < 0)
    panic("exynos: unknown timer IRQ for CPU%d\n", cxx::int_value<Cpu_number>(cpu));

  if (irq >= 32)
    {
      // SPI IRQ, assume each CPU has its own timer IRQ
      _timer_ticks.cpu(cpu).construct();
      _timer_ticks.cpu(cpu)->set_handler_mode(
          cpu == Cpu_number::boot_cpu() ? Sys_cpu : App_cpu);
      r = Irq_mgr::mgr->alloc(_timer_ticks.cpu(cpu), irq, false);
      Irq_mgr::mgr->set_cpu(irq, cpu);
    }
  else
    {
      // LPI IRQ a single IRQ number for all CPUs
      if (cpu != Cpu_number::boot_cpu())
        return;

      _timer_ticks.cpu(cpu).construct();
      _timer_ticks.cpu(cpu)->set_handler_mode(Any_cpu);
      r = Irq_mgr::mgr->alloc(_timer_ticks.cpu(cpu), irq, false);
    }

  if (!r)
    panic("Could not allocate scheduling IRQ %d for CPU%d\n", irq, cxx::int_value<Cpu_number>(cpu));
  else
    printf("Timer for CPU%d is at IRQ %d\n", cxx::int_value<Cpu_number>(cpu), irq);

  _timer_ticks.cpu(cpu)->_timer = Timer::timers.cpu(cpu).get();
}


