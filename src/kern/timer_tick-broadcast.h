#pragma once

#include <timer_tick_base.h>
#include <panic.h>
#include <cstdio>
#include <boot_alloc.h>
#include <ipi.h>
#include <irq_mgr.h>

template<typename TT>
class Timer_tick_broadcast : public Timer_tick_base<TT>
{
public:
  static TT *glbl_timer;

  static void setup(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      {
        glbl_timer = new Boot_object<TT>();
        glbl_timer->set_handler_mode(TT::Sys_cpu);
        if (!TT::allocate_irq(glbl_timer, Timer::irq()))
          panic("Could not allocate scheduling IRQ %d\n", Timer::irq());
        else
          printf("Timer is at IRQ %d\n", Timer::irq());

        glbl_timer->chip()->set_mode(glbl_timer->pin(), Timer::irq_mode());
      }
  }

  static void enable(Cpu_number)
  {
    glbl_timer->chip()->unmask(glbl_timer->pin());
  }

  static void disable(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      glbl_timer->chip()->mask(glbl_timer->pin());
    else
      {
        // disable IPI
      }
  }

  void ack()
  {
    Timer::acknowledge();
    Irq_base::ack();
    Ipi::bcast(Ipi::Timer, Cpu_number::boot_cpu());
  }

  // default to using normal IRQs allocated etc...
  static bool allocate_irq(Irq_base *irq, unsigned irqnum)
  { return Irq_mgr::mgr->alloc(irq, irqnum, false); }

#ifdef CONFIG_JDB
  static TT *boot_cpu_timer_tick()
  { return glbl_timer; }
#endif
};

template<typename TT>
TT *Timer_tick_broadcast<TT>::glbl_timer;
