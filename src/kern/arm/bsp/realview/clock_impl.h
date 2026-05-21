#include <globalconfig.h>
#ifndef CONFIG_PF_REALVIEW
#include_next <clock_impl.h>
#else

#include <io.h>
#include "platform_arm_realview.h"

class Clock_impl
{
public:
  typedef Unsigned64 Time;
  typedef Mword Counter;

  Clock_impl(Cpu_number) {}

  Cpu_time us(Time t) const
  {
    return t / 24;
  }

protected:
  Counter read_counter() const
  {
    return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz);
  }
};
#endif
