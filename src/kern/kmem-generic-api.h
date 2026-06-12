
#pragma once

#include <types.h>
#include <kmem_mmio.h>

class Kmem_generic_api
{
public:
  static Address mmio_remap(Address phys, Address size, bool cache = false, bool with_exec = false)
  {
    return Kmem_mmio::remap(phys, size, cache, with_exec);
  }
};
