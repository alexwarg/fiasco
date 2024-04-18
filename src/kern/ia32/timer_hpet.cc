
#include <timer_hpet.h>
#include <pit_i8254.h>
#include <hpet.h>
#include <cpu.h>
#include <config.h>

#include <cstdio>

int Timer_hpet::hpet_irq;

void
Timer_hpet::init(Cpu_number)
{
  hpet_irq = -1;
  if (!Hpet::init())
    return;

  hpet_irq = Hpet::int_num();
  if (hpet_irq == 0 && Hpet::int_avail(2))
    hpet_irq = 2;

  if (Config::Scheduler_one_shot)
    {
      // tbd
    }
  else
    {
      // setup hpet for periodic here
    }

  if (!Config::Scheduler_one_shot)
    // from now we can save energy in getchar()
    Config::getchar_does_hlt_works_ok = Config::hlt_works_ok;

  Hpet::enable();
  Hpet::dump();

  printf("Using HPET timer on IRQ %d (%s Mode) for scheduling\n",
         hpet_irq,
         Config::Scheduler_one_shot ? "One-Shot" : "Periodic");
}


