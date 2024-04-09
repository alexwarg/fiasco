#include <delayloop.h>

#include <kip.h>
#include <mem.h>
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
  Timer::update_timer(t1 + 1000); // 1ms

  // Execute code as similar to delay() as possible to get a reliable count.
  Mem::barrier();
  while (t1 == k->clock())
    {
      ++count;
      Proc::pause();
    }
  Mem::barrier();

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
 * Wait for a certain amount of time.
 *
 * \param ms  The number of milliseconds to wait for.
 *
 * Can be used in the kernel debugger while no timer tick is available. Don't
 * expect 100% accurate delays here.
 */
void
Delay::delay(unsigned ms)
{
  Kip *k = Kip::k();
  while (ms--)
    {
      // 'count' was determined by waiting for the KIP counter to change from
      // one value to the next value. Hence, 'count' has KIP counter granularity
      // -- which is currently 1ms.
      unsigned c = cyc_count;

      Mem::barrier();
      while (c--)
        {
          static_cast<void>(k->clock());
          Proc::pause();
        }
      Mem::barrier();
    }
}
