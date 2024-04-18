#include <timer_apic.h>

#include <apic.h>
#include <pit_i8254.h>
#include <std_macros.h>
#include <warn.h>
#include <kip.h>

#include <cstdio>

void
Timer_apic::init(Cpu_number)
{
  Apic::timer_assign_irq_vector(Config::Apic_timer_vector);

  if (Config::Scheduler_one_shot)
    {
      Apic::timer_set_one_shot();
      Apic::timer_reg_write(0xffffffff);
    }
  else
    {
      Apic::timer_set_periodic();
      Apic::timer_reg_write(Apic::us_to_apic(Config::Scheduler_granularity));
    }

  // make sure that PIT does pull its interrupt line
  Pit::done();

  if (!Config::Scheduler_one_shot)
    // from now we can save energy in getchar()
    Config::getchar_does_hlt_works_ok = false && Config::hlt_works_ok;

  if (Warn::is_enabled(Info))
    printf ("Using the Local APIC timer on vector %x (%s Mode) for scheduling\n",
            (unsigned)Config::Apic_timer_vector,
            Config::Scheduler_one_shot ? "One-Shot" : "Periodic");

}

void
Timer_apic::update_one_shot(Unsigned64 wakeup)
{
  Unsigned32 apic;
  Unsigned64 now = Kip::k()->clock();
  if (EXPECT_FALSE (wakeup <= now))
    // already expired
    apic = 1;
  else
    {
      Unsigned64 delta = wakeup - now;
      if (delta < Config::One_shot_min_interval_us)
        apic = Apic::us_to_apic(Config::One_shot_min_interval_us);
      else if (delta > Config::One_shot_max_interval_us)
        apic = Apic::us_to_apic(Config::One_shot_max_interval_us);
      else
        apic = Apic::us_to_apic(delta);

      if (EXPECT_FALSE (apic < 1))
        // timeout too small
        apic = 1;
    }

  Apic::timer_reg_write(apic);
}

