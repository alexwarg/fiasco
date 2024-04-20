#pragma once

#include <types.h>

class Tlb_entry
{
public:
  enum : Mword
  {
    Global     = 0x001,
    Valid      = 0x002,
    Dirty      = 0x004,
    Write      = Dirty,
    Cache_mask = 0x038,
    Uncached   = 0x010,
    // uf UCA supported: C_UCA      = 0x038,
    C_UCA      = 0x010, // fallback to uncached
  };

  static Mword cached;
};

