#pragma once

#include <timer_tick_base.h>
#include <apic.h>

class Timer_tick_apic : public Timer_tick_base<Timer_tick_apic>
{
public:
  static void setup(Cpu_number)
  {}

  static void enable(Cpu_number)
  {
    Apic::timer_enable_irq();
    Apic::irq_ack();
  }

  static void disable(Cpu_number)
  {
    Apic::timer_disable_irq();
  }

  static void ack()
  {
    Apic::irq_ack();
  }

  static void set_vectors_stop();
};
