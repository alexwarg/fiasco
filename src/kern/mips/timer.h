#pragma once

#include <timer_mips.h>
#include <irq_chip.h>
#include <system_clock.h>
#include <per_cpu_data.h>
#include <types.h>

class Timer
{
public:
  static unsigned irq() { return Timer_mips::irq(); }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      System_clock::init();

    _timer.cpu(cpu).construct(cpu);
  }

  static void acknowledge()
  {
    _timer.current()->acknowledge();
  }

  static Unsigned64 get_current_counter()
  {
    return _timer.current()->get_current_counter();
  }

private:
  static Per_cpu<Static_object<Timer_mips> > _timer;
};
