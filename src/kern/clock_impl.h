#pragma once

// generic KIP based clock

#include <system_clock.h>
#include <l4_types.h>

class Clock_impl
{
public:
  typedef Unsigned64 Time;
  typedef Cpu_time Counter;

  Clock_impl(Cpu_number) {}

  Cpu_time us(Time t) const
  {
    return t;
  }

protected:
  Counter read_counter() const
  {
    return System_clock::clock();
  }
};

