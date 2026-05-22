#include "mem_layout.h"

#include <cstdio>

#ifdef CONFIG_NONCONT_MEM

unsigned short Mem_layout_arch::__ph_to_pm[1 << (32 - Config::SUPERPAGE_SHIFT)];

#endif

Address
Mem_layout_arch::pmem_to_phys(Address addr)
{
  printf("Mem_layout::pmem_to_phys(Address addr=%lx) is not implemented\n", addr);
  return 0;
}
