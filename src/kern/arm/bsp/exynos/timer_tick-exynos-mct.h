#pragma once

#include <timer_tick_base.h>
#include <timer.h>
#include <per_cpu_data.h>
#include <types.h>

class Timer_tick_exynos_mct : public Timer_tick_base<Timer_tick_exynos_mct>
{
public:
  static Per_cpu<Static_object<Timer_tick_exynos_mct> > _timer_ticks;

  static void setup(Cpu_number cpu);

  static void enable(Cpu_number cpu)
  {
    auto &t = *_timer_ticks.cpu(cpu).get();
    t.chip()->unmask(t.pin());
  }

  static void disable(Cpu_number cpu)
  {
    auto &t = *_timer_ticks.cpu(cpu).get();
    t.chip()->mask(t.pin());
  }

  void ack()
  {
    _timer->acknowledge();
    Irq_base::ack();
  }

#ifdef CONFIG_JDB
  static Timer_tick_exynos_mct *boot_cpu_timer_tick()
  { return _timer_ticks.cpu(Cpu_number::boot_cpu()); }
#endif

private:
  Timer *_timer;
};
