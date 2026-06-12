
#include <timer_integrator.h>
#include <kmem_mmio.h>
#include <mem_layout.h>

Static_object<Timer_integrator> Timer_integrator::_timer;

Timer_integrator::Timer_integrator(void *base)
  : Mmio_register_block(base)
{
  /* Switch all timers off */
  write(0u, TIMER0_BASE + TIMER_CTRL);
  write(0u, TIMER1_BASE + TIMER_CTRL);
  write(0u, TIMER2_BASE + TIMER_CTRL);

  unsigned timer_ctrl = TIMER_CTRL_ENABLE | TIMER_CTRL_PERIODIC;
  unsigned timer_reload = 1000000 / Config::Scheduler_granularity;

  write(timer_reload, TIMER1_BASE + TIMER_LOAD);
  write(timer_reload, TIMER1_BASE + TIMER_VALUE);
  write(timer_ctrl | TIMER_CTRL_IE, TIMER1_BASE + TIMER_CTRL);
}

void
Timer_integrator::init(Cpu_number)
{
  _timer.construct(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x300));
}
