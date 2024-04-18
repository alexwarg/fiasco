#include <timer.h>
#include <system_clock.h>

Static_object<Tcu_jz4780> Timer::_tcu;

void
Timer::init(Cpu_number ncpu)
{
  typedef Tcu_jz4780 T;
  if (ncpu != Cpu_number::boot_cpu())
    return;

  System_clock::init();
  _tcu.construct(0xb0002000);
  _tcu->r[T::TSCR]    = 1 << 15;
  _tcu->r[T::OSTCSR]  = 2 << 3;  // prescaler 16, periodic mode
  _tcu->r[T::OSTCNTH] = 0;
  _tcu->r[T::OSTCNTL] = 0;
  _tcu->r[T::OSTCSR].set(4);     // set EXT_EN
  _tcu->r[T::OSTDR]   = 48000000 / 16 / Config::Scheduler_granularity;
}

void
Timer::enable()
{
  typedef Tcu_jz4780 T;
  _tcu->r[T::OSTCNTH] = 0;
  _tcu->r[T::OSTCNTL] = 0;
  _tcu->r[T::TFCR]    = 1 << 15;
  _tcu->r[T::TMCR]    = 1 << 15; // clear OSTMCL
  _tcu->r[T::TESR]    = 1 << 15; // set OSTEN
}


