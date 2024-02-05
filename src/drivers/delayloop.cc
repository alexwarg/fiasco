#include <delayloop.h>

#include <kip.h>
#include <processor.h>
#include <timer.h>

static unsigned cyc_count;

static unsigned measure()
{
  Cpu_time t1;
  unsigned count = 0;

  Kip *k = Kip::k();
  Cpu_time t = k->clock();
  Timer::update_timer(t + 1000); // 1ms
  while (t == (t1 = k->clock()))
    Proc::pause();
  Timer::update_timer(k->clock() + 1000); // 1ms
  while (t1 == k->clock())
    {
      ++count;
      Proc::pause();
    }

  return count;
}

void
Delay::init()
{
  cyc_count = measure();
  unsigned c2 = measure();
  if (c2 > cyc_count)
    cyc_count = c2;
}

/**
 * Hint: ms is actually the timer granularity, which
 *       currently happens to be milliseconds
 */
void
Delay::delay(unsigned ms)
{
  Kip *k = Kip::k();
  while (ms--)
    {
      unsigned c = cyc_count;
      while (c--)
        {
          static_cast<void>(k->clock());
          Proc::pause();
        }
    }
}
