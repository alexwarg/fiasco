
#include <paging-ptab.h>

Mword Tlb_entry::cached;

Address
Pdir::virt_to_phys(Address virt) const
{
  Mword const *p = _entries;
  Mword v = 0;
  Mword field;

  for (unsigned lvl = 0; lvl < 4; ++lvl)
    {
      auto const size = l_size(lvl);
      if (!size)
        continue;

      field = l_field(lvl);
      Mword const mask = (1UL << size) - 1;
      auto idx = (virt >> field) & mask;
      v = p[idx];
      if (v & Leaf)
        break; // this was the last level

      p = reinterpret_cast<Mword const *>(v);
    }

  if (!(v & Valid))
    return ~0UL;

  v = (v << (6 - PWField_ptei)) & (~0UL << 12);
  v |= virt & ((1UL << field) - 1);
  return v;
}


