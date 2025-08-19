#pragma once

#include <irq_chip.h>
#include <generic_timer.h>
#include <std_macros.h>

class Timer_generic_timer
{
private:
  enum
  {
    CTL_ENABLE   = 1 << 0,
    CTL_IMASK    = 1 << 1,
    CTL_ISTATUS  = 1 << 2,
  };

public:
  typedef Generic_timer::Gtimer Gtimer;
  static constexpr Unsigned64 Infinite_timeout = UINT64_MAX;

  static void init(Cpu_number cpu)
  {
    if (!check_and_disable(cpu))
      return;
    finalize_init(cpu);
  }

  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 29;
      case Generic_timer::Virtual:  return 27;
      case Generic_timer::Hyp:      return 26;
      case Generic_timer::Secure_hyp: return 20;
      };
  }

  static Irq_chip::Mode irq_mode()
  {
    // Some sources describe this IRQ as "level/low" but the GIC code only allows
    // "level/high" or "edge/high". The GIC redistributor doesn't distinguish
    // between "low" and "high" so just use the accepted level-sensitive value.
    return Irq_chip::Mode::F_level_high;
  }

  static void enable()
  {
    Gtimer::compare(Gtimer::counter() + _interval);
    Gtimer::control(CTL_ENABLE);
  }

  static void acknowledge()
  {
    if constexpr (!Config::Scheduler_one_shot)
      Gtimer::compare(Gtimer::compare() + _interval);
    // else done in Timer::update_timer()
  }

  static void update_timer(Unsigned64 wakeup)
  {
    if constexpr (Config::Scheduler_one_shot)
      update_timer_one_shot(wakeup);
  }

protected:
  static bool check_and_disable(Cpu_number cpu);
  static void finalize_init(Cpu_number cpu);

  static void update_timer_one_shot(Unsigned64 wakeup);

  static void set_freq0(Mword freq0)
  {
    _freq0 = freq0;
    Gtimer::frequency(freq0);
  }

  static bool is_boot_cpu(Cpu_number cpu)
  {
    return !IS_ENABLED(CONFIG_MP) || (cpu == Cpu_number::boot_cpu());
  }

  static Mword _interval;
  static Mword _freq0;
};

