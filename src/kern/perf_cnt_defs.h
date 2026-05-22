#pragma once

#include "types.h"

namespace Perf_cnt
{
  enum
  {
    Max_slot = 2,
    Max_pmc  = 4,
  };

  enum Unit_mask_type
  { None, Fixed, Exclusive, Bitmask, };

  typedef Mword (*Perf_read_fn)();

  extern Perf_read_fn read_pmc[Max_slot];
};
