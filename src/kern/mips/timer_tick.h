#pragma once

// default to cp0 timer
#include <timer_tick-single-vector.h>
#include <mips_cpu_irqs.h>

struct Timer_tick : Timer_tick_single_vector<Timer_tick>
{
  static bool allocate_irq(Irq_base *irq, unsigned cpu_irq)
  {
    // ignore double alloc of the timer for CPU local IRQs
    if (   (irq->chip() == Mips_cpu_irqs::chip)
        && (irq->pin() == cpu_irq))
      return true;

    return Mips_cpu_irqs::chip->alloc(irq, cpu_irq);
  }
};
