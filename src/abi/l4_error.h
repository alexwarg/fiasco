#pragma once

#include "types.h"

class L4_error
{
public:
  enum Error_code
  {
    None            = 0,
    Timeout         = 2,
    R_timeout       = 3,
    Not_existent    = 4,
    Canceled        = 6,
    R_canceled      = 7,
    Overflow        = 8,
    Snd_xfertimeout = 10,
    Rcv_xfertimeout = 12,
    Aborted         = 14,
    R_aborted       = 15,
    Map_failed      = 16,
  };

  enum Phase
  {
    Snd = 0,
    Rcv = 1
  };

  constexpr L4_error(Error_code ec = None, Phase p = Snd) : _raw(ec | p) {}
  constexpr L4_error(L4_error const &e, Phase p) : _raw(e._raw | p) {}

  bool ok() const noexcept
  { return (_raw & 0xff) == 0; }

  Error_code error() const noexcept
  { return Error_code(_raw & 0x1f); }

  Mword raw() const noexcept
  { return _raw; }

  bool snd_phase() const noexcept
  { return !(_raw & Rcv); }

  bool empty_map() const noexcept
  { return _raw & 0x100; }

  void set_empty_map() noexcept
  { _raw |= 0x100; }

  constexpr static L4_error from_raw(Mword raw) noexcept
  { return L4_error(true, raw); }

  char const *str_error() const;

private:
  constexpr L4_error(bool, Mword raw) : _raw(raw) {}
  Mword _raw;
};

