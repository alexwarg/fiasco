#pragma once

#include <timer_tick_base.h>
#include <timer.h>
#include <panic.h>
#include <boot_alloc.h>
#include <irq_mgr.h>

template<typename TT, bool INIT_IRQ = false>
class Timer_tick_single_vector : public Timer_tick_base<TT>
{
private:
  using Base = Timer_tick_base<TT>;

public:
  static void setup(Cpu_number cpu)
  {
    // all CPUs use the same timer IRQ, so initialize just on CPU 0
    if (cpu == Cpu_number::boot_cpu())
      {
        glbl_timer = new Boot_object<TT>();
        glbl_timer->set_handler_mode(Base::Any_cpu);
      }

    if (!TT::allocate_irq(glbl_timer, Timer::irq()))
      panic("Could not allocate scheduling IRQ %d\n", Timer::irq());

    glbl_timer->chip()->set_mode_percpu(cpu, glbl_timer->pin(),
                                        Timer::irq_mode());
  }

  static void enable(Cpu_number cpu)
  {
    glbl_timer->chip()->unmask_percpu(cpu, glbl_timer->pin());
    Timer::enable();
  }

  static void disable(Cpu_number cpu)
  {
    glbl_timer->chip()->mask_percpu(cpu, glbl_timer->pin());
  }

  void ack()
  {
    Timer::acknowledge();
    Irq_base::ack();
  }

  // default to using normal IRQs allocated etc...
  static bool allocate_irq(Irq_base *irq, unsigned irqnum)
  { return Irq_mgr::mgr->alloc(irq, irqnum, INIT_IRQ); }

#ifdef CONFIG_JDB
  static TT *boot_cpu_timer_tick()
  { return glbl_timer; }
#endif

  static TT *glbl_timer;
};

template<typename TT, bool INIT_IRQ>
TT *Timer_tick_single_vector<TT, INIT_IRQ>::glbl_timer;
