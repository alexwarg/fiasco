#pragma once

#include <rcupdate.h>
#include <cpu_idle_iface.h>
#include <timeout.h>
#include <timer_tick.h>
#include <mem_space.h>
#include <mem_unit.h>
#include <lock_guard.h>
#include <cpu_lock.h>
#include <processor.h>

class Cpu_tickless_idle_base
{
public:
  static Per_cpu<unsigned long> idle_counter;
  static Per_cpu<unsigned long> deep_idle_counter;

};

struct Cpu_tickless_idle_default
{
  void arch_tickless_idle(Cpu_number)
  {
    Proc::halt();
  }

  void arch_idle(Cpu_number)
  {
    Proc::halt();
  }
};

template<typename IMPL = Cpu_tickless_idle_default>
class Cpu_tickless_idle : public Cpu_idle_iface, private IMPL, private Cpu_tickless_idle_base
{
public:
  void idle() override
  {
    // this version must run with disabled IRQs and a wakeup must continue
    // directly after the wait for event.
    auto guard = lock_guard(cpu_lock);
    auto cpu = current_cpu();
    ++idle_counter.cpu(cpu);
    // 1. check for latency requirements that prevent low power modes
    // 2. check for timeouts on this CPU ignore the idle thread's timeslice
    // 3. check for RCU work on this CPU
    if (Rcu::idle(cpu)
        && !Timeout_q::timeout_queue.cpu(cpu).have_timeouts(timeslice_timeout.cpu(cpu)))
      {
        ++deep_idle_counter.cpu(cpu);
        Rcu::enter_idle(cpu);
        Timer_tick::disable(cpu);
        Mem_space::disable_tlb(cpu);
        Mem_unit::tlb_flush();

        // do everything to do to a deep sleep state:
        //  - flush caches
        //  - ...
        this->arch_tickless_idle(cpu);

        Mem_space::enable_tlb(cpu);
        Rcu::leave_idle(cpu);
        Timer_tick::enable(cpu);
      }
    else
      this->arch_idle(cpu);
  }
};
