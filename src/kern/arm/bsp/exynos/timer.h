#pragma once

#include <globalconfig.h>

#ifdef CONFIG_ARM_GENERIC_TIMER
#include_next <timer.h>
#endif

#ifdef CONFIG_PF_EXYNOS_TIMER_MP

#include <timer_arm_mptimer.h>

struct Timer : Timer_arm_mptimer
{
  static void init(Cpu_number cpu);
};

#endif

#ifdef CONFIG_PF_EXYNOS_TIMER_MCT

#include <timer_mct.h>
#include <per_cpu_data.h>
#include <kip.h>

struct Timer : Mct_core_timer
{
  explicit Timer(Address virt) : Mct_core_timer(virt) {}
  static Static_object<Mct_timer> mct;
  static Per_cpu<Static_object<Timer> > timers;

  static void init(Cpu_number cpu);

  static void update_timer(Unsigned64 wakeup)
  {
    if (!Config::Scheduler_one_shot)
      return;

    Unsigned64 now = Kip::k()->clock();
    Mword interval_mct;
    if (EXPECT_FALSE(wakeup <= now))
      interval_mct = 1;
    else
      interval_mct = us_to_mct(wakeup - now);

    timers.cpu(current_cpu())->set_interval(interval_mct);
  }

private:
  static unsigned us_to_mct(Unsigned64 d_us)
  {
    if (d_us > Maxinterval_us)
      return Maxinterval_mct;

    return d_us * (Mct_freq / 1000000);
  }
};

#endif

#ifdef CONFIG_PF_EXYNOS_TIMER_PWM

#include <timer_arm_s3c2410.h>
#include <mem_layout.h>
#include <irq_chip.h>

struct Timer : Timer_arm_s3c2410
{
  enum { Reload_value = 66666 };

  static unsigned irq() { return 68 + Timer_nr; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void init(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      _timer.construct(Mem_layout::Pwm_phys_base, true, Reload_value);
  }

  static void acknowledge()
  {
    _timer->acknowledge_cint();
  }

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

};

#endif
