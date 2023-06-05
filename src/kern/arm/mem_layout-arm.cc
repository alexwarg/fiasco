#include "mem_layout.h"

#include <cstdio>

Mem_layout_arch::Pmem_region Mem_layout_arch::_pm_regions[Mem_layout_arch::Max_pmem_regions];
unsigned Mem_layout_arch::_num_pm_regions;

bool
Mem_layout_arch::add_pmem(Address phys, Address virt, unsigned long size)
{
  if (size == 0)
    return false;

  if (_num_pm_regions >= Max_pmem_regions)
    return false;

  --size;

  // The pmem map must be unambiguous in either direction. Make sure nothing
  // intersects with existing records.
  for (unsigned i = 0; i < _num_pm_regions; ++i)
    {
      if (   phys + size >= _pm_regions[i].paddr
          && phys        <= _pm_regions[i].paddr + _pm_regions[i].size)
        return false;

      if (   virt + size >= _pm_regions[i].vaddr
          && virt        <= _pm_regions[i].vaddr + _pm_regions[i].size)
        return false;
    }

  _pm_regions[_num_pm_regions].paddr = phys;
  _pm_regions[_num_pm_regions].vaddr = virt;
  _pm_regions[_num_pm_regions].size = size;
  _num_pm_regions++;

  return true;
}

