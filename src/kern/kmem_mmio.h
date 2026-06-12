
#pragma once

#include <types.h>


namespace Kmem_mmio
{
  static constexpr uintptr_t invalid_ptr = ~static_cast<uintptr_t>(0);
  void *map(Address phys, size_t size, bool cache = false,
            bool exec = false, bool global = true);
  void unmap(void *ptr, size_t size);


  inline Address
  remap(Address phys, Address size, bool cache = false, bool exec = false)
  {
    return reinterpret_cast<Address>(map(phys, size, cache, exec));
  }
}
