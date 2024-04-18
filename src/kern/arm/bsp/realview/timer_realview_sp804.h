#pragma once

#include <timer_sp804.h>
#include <irq_chip.h>
#include <types.h>
#include <globalconfig.h>

struct Timer_realview_sp804
{
#ifdef CONFIG_PF_REALVIEW_VEXPRESS_A15
  static unsigned irq() { return 34; }
#else
  static unsigned irq() { return 36; }
#endif

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wkaueup*/)
  {}

  static void init(Cpu_number cpu);

  static void acknowledge()
  {
    sp804->irq_clear();
  }

private:
  static Static_object<Timer_sp804> sp804;
};

