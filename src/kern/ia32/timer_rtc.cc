
#include <timer_rtc.h>
#include <rtc-ia32.h>
#include <pit_i8254.h>

#include <cstdio>

void
Timer_rtc::init(Cpu_number)
{
  printf("Using the RTC on IRQ %d (%sHz) for scheduling\n", 8,
#ifdef CONFIG_SLOW_RTC
         "64"
#else
         "1k"
#endif
      );

  // set up timer interrupt (~ 1ms)
  Rtc::init();

  // make sure that PIT does pull its interrupt line
  Pit::done();
}

