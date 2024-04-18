
#include <timer_pit.h>
#include <pit_i8254.h>

#include <cstdio>

void
Timer_pit::init(Cpu_number)
{
  printf("Using the PIT (i8254) on IRQ %d for scheduling\n", irq());

  // set up timer interrupt (~ 1ms)
  Pit::init();
}

