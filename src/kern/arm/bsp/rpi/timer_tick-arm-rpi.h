#pragma once

#include <timer_tick_base.h>
#include <timer.h>
#include <arm_control.h>

class Timer_tick_arm_rpi : public Timer_tick_base<Timer_tick_arm_rpi>
{
public:
  static void setup(Cpu_number)
  {}

  static void enable(Cpu_number)
  {
    Arm_control::o()->timer_unmask(Timer::irq());
    Timer::enable();
  }

  static void disable(Cpu_number)
  {
    Arm_control::o()->timer_mask(Timer::irq());
  }

  static void ack()
  {
    Timer::acknowledge();
  }
};
