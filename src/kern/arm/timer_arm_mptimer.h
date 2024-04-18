#pragma once

#include <types.h>
#include <irq_chip.h>
#include <scu.h>
#include <cpu.h>

struct Timer_arm_mptimer
{
  static unsigned irq() { return 29; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {
    static_assert(!Config::Scheduler_one_shot,
                  "currently no dynamic ticks with ARM generic timer");
  }

  static void acknowledge()
  {
    Cpu::scu->write<Mword>(Timer_int_stat_event, Timer_int_stat_reg);
  }

protected:
  enum
  {
    Timer_load_reg     = 0x600 + 0x0,
    Timer_counter_reg  = 0x600 + 0x4,
    Timer_control_reg  = 0x600 + 0x8,
    Timer_int_stat_reg = 0x600 + 0xc,

    Prescaler = 0,

    Timer_control_enable    = 1 << 0,
    Timer_control_reload    = 1 << 1,
    Timer_control_itenable  = 1 << 2,
    Timer_control_prescaler = (Prescaler & 0xff) << 8,

    Timer_int_stat_event   = 1,
  };

  static Mword start_as_counter()
  {
    static_assert(Scu::Available, "No SCU available in this configuration");

    Mword v = ~0UL;
    Cpu::scu->write<Mword>(v, Timer_counter_reg);

    Cpu::scu->write<Mword>(Timer_control_prescaler | Timer_control_reload
                           | Timer_control_enable,
                           Timer_control_reg);
    return v;
  }

  static Mword stop_counter()
  {
    Mword v = Cpu::scu->read<Mword>(Timer_counter_reg);
    Cpu::scu->write<Mword>(0, Timer_control_reg);
    return v;
  }

  static void init(Mword interval)
  {

    Mword i = interval;

    Cpu::scu->write<Mword>(i, Timer_load_reg);
    Cpu::scu->write<Mword>(i, Timer_counter_reg);
    Cpu::scu->write<Mword>(Timer_control_prescaler | Timer_control_reload
                           | Timer_control_enable | Timer_control_itenable,
                           Timer_control_reg);
  }
};

template<Mword INTERVAL>
struct Timer_arm_mptimer_t : Timer_arm_mptimer
{
  static void init(Cpu_number)
  { Timer_arm_mptimer::init(INTERVAL); }
};

