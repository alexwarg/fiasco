#pragma once

#include <types.h>
#include <irq_chip.h>
#include <arm_mptimer.h>
#include <cpu.h>

struct Timer_arm_mptimer
{
  static Arm_mptimer mptimer()
  {
    return Cpu::scu.mptimer();
  }

  static unsigned irq() { return 29; }

  static Irq_chip::Mode irq_mode()
  {
    return Irq_chip::Mode::F_raising_edge;
  }

  static void enable()
  {
    mptimer().ack();
  }

  static void update_timer(Unsigned64 /*wakeup*/)
  {
    static_assert(!Config::Scheduler_one_shot,
                  "currently no dynamic ticks with ARM generic timer");
  }

  static void acknowledge()
  {
    mptimer().ack();
  }

protected:
  static void init(Mword interval)
  {
    mptimer().init_periodic(interval);
  }
};

template<Mword INTERVAL>
struct Timer_arm_mptimer_t : Timer_arm_mptimer
{
  static void init(Cpu_number)
  { Timer_arm_mptimer::init(INTERVAL); }
};

