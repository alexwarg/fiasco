
#pragma once

#include <cpu.h>
#include <config.h>

class Timer_mips
{
public:
  static unsigned irq()
  { return (Mips::mfc0_32(12, 1) >> 29) & 0x7; }

  explicit Timer_mips(Cpu_number ncpu)
  {
    _current_cmp = _get_counter();
    _last_counter = _current_cmp;
    _counter_high = 0;
    // Timer is clocked with half the CPU frequency
    Cpu &cpu = Cpu::cpus.cpu(ncpu);
    _interval = (cpu.frequency() / 1000000) * (Config::Scheduler_granularity / 2);

    if (true) // interval mode
      _current_cmp += _interval;

    for (;;)
      {
        _set_compare(_current_cmp);
        // See explanation in Timer::acknowledge().
        Unsigned32 cnt = _get_counter();
        if (EXPECT_TRUE((Signed32)(_current_cmp - cnt) > 0))
          break;
        _current_cmp = cnt + Adj_time;
        _last_counter = _current_cmp;
      }
  }

  void acknowledge()
  {
    if (true) // interval mode
      _current_cmp += _interval;

    Unsigned32 cnt = _get_counter();
    if (cnt < _last_counter)
      ++_counter_high;

    _last_counter = cnt;

    Unsigned32 new_cmp = _current_cmp;
    for (;;)
      {
        // clear TI bit and set new value if applicable
        _set_compare(new_cmp);
        // Now verify that the compare register was indeed set beyond the counter
        // because otherwise it will take a complete wrap around of the counter
        // until the next timer interrupt is generated. This could happen on QEMU
        // or in a VM due to preemption but also on real hardware after we
        // entered + left the kernel debugger. In the latter case we probably
        // need several timer periods until _current_cmp got in sync with the
        // counter again. Note that we don't update _current_cmp here because
        // otherwise we would skip timer interrupts.
        cnt = _get_counter();
        if (EXPECT_TRUE((Signed32)(new_cmp - cnt) > 0))
          break;
        new_cmp = cnt + Adj_time;
      }

    // we don't care about CP0 hazards here as a possible IRQ
    // enable afterwards will clear those anyways
    // printf("TI: %u %u %u\n", cnt, t->_current_cmp, t->_interval);
  }

  Unsigned64 get_current_counter() const
  {
    Unsigned32 cc = _get_counter();
    Unsigned32 hi = _counter_high;
    if (cc < _last_counter)
      ++hi;

    return (((Unsigned64)hi) << 32) | cc;
  }

private:
  Unsigned32 _interval;
  Unsigned32 _current_cmp;
  Unsigned32 _last_counter;
  Unsigned32 _counter_high;

  // Chose a reasonable amount of cycles within that we will be able to
  // reprogram the compare register without being 'caught up' by the counter
  // register.
  static constexpr long Adj_time = 8000;

  static Unsigned32 _get_compare()
  { return Mips::mfc0_32(Mips::Cp0_compare); }

  static void _set_compare(Unsigned32 v)
  { Mips::mtc0_32(v, Mips::Cp0_compare); }

  static Unsigned32 _get_counter()
  { return Mips::mfc0_32(Mips::Cp0_count); }

  static void _set_counter(Unsigned32 cnt)
  { Mips::mtc0_32(cnt, Mips::Cp0_count); }
};
