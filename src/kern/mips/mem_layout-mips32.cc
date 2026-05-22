#include "mem_layout.h"

Address
Mem_layout_arch::ioremap_nocache(Address phys_addr, Address size)
{
  Address last_addr;

  /* Don't allow wraparound or zero size */
  last_addr = phys_addr + size - 1;
  if (!size || last_addr < phys_addr)
    return 0;

  /*
   * Map uncached objects in the low 512mb of address space using KSEG1.
   */
  if (below_512mb(last_addr))
    return KSEG1 + phys_addr;

  return 0;
}
