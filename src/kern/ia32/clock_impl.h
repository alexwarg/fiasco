#pragma once

#include <l4_types.h>
#include <cpu.h>

class Clock_impl
{
public:
  typedef Unsigned64 Counter;
  typedef Unsigned64 Time;

  Clock_impl(Cpu_number n) : _cpu_id(n) {}

  Cpu_time us(Time t) const
  {
    return Cpu::cpus.cpu(_cpu_id).tsc_to_us(t);
  }

protected:
  Counter read_counter() const
  {
    return Cpu::rdtsc();
  }

private:
  Cpu_number _cpu_id;
};
