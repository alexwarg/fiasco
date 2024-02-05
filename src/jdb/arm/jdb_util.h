#pragma once

#include "kmem.h"

class Jdb_util
{
public:
  static bool is_mapped(void const *addr)
  {
    return Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(addr)) != static_cast<Address>(~0UL);
  }
};
