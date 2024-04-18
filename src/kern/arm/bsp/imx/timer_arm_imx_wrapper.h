#pragma once

#include <irq_chip.h>
#include <types.h>


template<typename IMPL, unsigned IRQ, Address ...PHYS_BASE>
class Timer_arm_imx_wrapper
{
public:
  static unsigned irq() { return IRQ; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void acknowledge()
  {
    _timer->acknowledge();
  }

  static void init(Cpu_number)
  {
    _timer.construct(PHYS_BASE...);
  }

private:
  static Static_object<IMPL> _timer;
};

template<typename IMPL, unsigned IRQ, Address ...PHYS_BASE>
Static_object<IMPL> Timer_arm_imx_wrapper<IMPL, IRQ, PHYS_BASE...>::_timer;

