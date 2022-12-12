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

  _pm_regions[_num_pm_regions].paddr = phys;
  _pm_regions[_num_pm_regions].vaddr = virt;
  _pm_regions[_num_pm_regions].size = size - 1U;
  _num_pm_regions++;

  return true;
}

