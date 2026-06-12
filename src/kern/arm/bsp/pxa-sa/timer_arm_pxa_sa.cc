#include <timer.h>

#include <kmem_mmio.h>
#include <mem_layout.h>
#include <kip.h>

Static_object<Timer> Timer::_timer;

Timer::Timer()
: Mmio_register_block(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x20))
{
  write<Mword>(1,          OIER); // enable OSMR0
  write<Mword>(0,          OWER); // disable Watchdog
  write<Mword>(Timer_diff, OSMR0);
  write<Mword>(0,          OSCR); // set timer counter to zero
  write<Mword>(~0U,        OSSR); // clear all status bits
}

#if 0
inline
Unsigned64
Timer::timer_to_us(Unsigned32 cr)
{ return (((Unsigned64)cr) << 14) / 60398; }

inline
Unsigned64
Timer::us_to_timer(Unsigned64 us)
{ return (us * 60398) >> 14; }

inline
void
Timer::ack()
{
  if (Config::Scheduler_one_shot)
    {
      Kip::k()->add_to_clock(timer_to_us(read<Unsigned32>(OSCR)));
      //puts("Reset timer");
      write<Mword>(0, OSCR);
      write<Mword>(0xffffffff, OSMR0);
    }
  else
    write<Mword>(0, OSCR);
  write<Mword>(1, OSSR); // clear all status bits

  // hmmm?
  //enable();
}

void
Timer::acknowledge()
{
  _timer->ack();
}


#endif
