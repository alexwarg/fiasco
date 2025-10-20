#pragma once

// dummy for JDB
struct Arch_time_source
{
  static Unsigned64 ts_to_ns(Unsigned64 ts)
  { return ts; }

  constexpr static bool Ts_to_ns_woks = false;
};

