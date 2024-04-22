#pragma once

#include <timer_tick-single-vector.h>

class Timer_tick_ia32 : public Timer_tick_single_vector<Timer_tick_ia32>
{
public:
  static bool allocate_irq(Irq_base *irq, unsigned irqnum);
  static void set_vectors_stop();
};
