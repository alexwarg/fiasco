#pragma once

#include <clock_impl.h> // implementation from BSP or arch etc.

class Clock : public Clock_impl
{
public:
  typedef Unsigned64 Time;

  Clock(Cpu_number cpu)
  : Clock_impl(cpu), _last_value(read_counter())
  {}

  Time delta()
  {
    Counter t = read_counter();
    Counter r = t - _last_value;
    _last_value = t;
    return Time(r);
  }

private:
  Counter _last_value;
};

