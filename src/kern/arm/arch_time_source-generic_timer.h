#pragma once

#include <generic_timer.h>
#include <fix_point_multiplier.h>
#include <globalconfig.h>

struct Arch_time_source_generic_timer
{
  using Gtimer = Generic_timer::Gtimer;

  static Cpu_time time_us()
  { return ts_to_us(time_stamp()); }

  static void init_system_clock();

  static Unsigned64 time_stamp()
  { return Gtimer::counter(); }

  static Unsigned64 ts_to_ns(Unsigned64 ts)
  { return timer_value_to_time(ts, _scaler_shift_ts_to_ns); }

  static Unsigned64 ts_to_us(Unsigned64 ts)
  { return timer_value_to_time(ts, _scaler_shift_ts_to_us); }

  static void setup_scalers(Unsigned32 freq);

private:
  static Fix_point_multiplier _scaler_shift_ts_to_ns;
  static Fix_point_multiplier _scaler_shift_ts_to_us;

  static Unsigned64
  timer_value_to_time(Unsigned64 v, Fix_point_multiplier scaler_shift)
  {
    return scaler_shift * v;
  }
};
