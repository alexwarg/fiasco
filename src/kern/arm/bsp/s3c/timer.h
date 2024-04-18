#pragma once

#include <timer_arm_s3c2410.h>
#include <mem_layout.h>
#include <irq_chip.h>

struct Timer : Timer_arm_s3c2410
{
  enum { Reload_value = 33333 };

  static unsigned irq() { return 10 + Timer_nr; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      _timer.construct(Mem_layout::Pwm_phys_base, false, Reload_value);
  }

  static void acknowledge()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

};


