
#pragma once

#include <cstdint>

/**
 * Determine scaling factor and shift value for transforming a time stamp
 * (timer value) into a time value (microseconds or nanoseconds).
 *
 * \param period  Time period: 10^6: microseconds; 10^9: nanoseconds.
 * \param freq    Timer frequency.
 * \param scaler  Determined scaling factor (32-bit).
 * \param shift   Determined shift value (0-31).
 *
 * The following formula is used to translate a timer value into a time value:
 *
 * \code
 *             timer value * scaler                 timer value * scaler
 *   time  =  ---------------------- * 2^shift  =  ---------------------
 *                     2^32                             2^(32-shift)
 * \endcode
 *
 * The shift value is important for low timer frequencies to keep a sane amount
 * of usable digits.
 */
class Fix_point_multiplier
{
public:
  uint32_t scaler;
  uint32_t shift;

  Fix_point_multiplier() = default;

  static constexpr Fix_point_multiplier
  calc(uint32_t from, uint32_t to)
  {
    uint32_t shift = 0;
    while (shift < 31 && (to / (1 << shift)) / from > 0)
      ++shift;
    uint32_t scaler = (((1ULL << 32) / (1ULL << shift)) * to) / from;
    return Fix_point_multiplier{scaler, shift};
  }

  uint64_t multiply(uint64_t v) const;

  friend uint64_t operator * (uint64_t v, Fix_point_multiplier const &m)
  { return m.multiply(v); }

  friend uint64_t operator * (Fix_point_multiplier const &m, uint64_t v)
  { return m.multiply(v); }
};

#include <fix_point_multiplier_arch.h>
