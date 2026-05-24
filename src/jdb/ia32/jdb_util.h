#pragma once

#include <kmem.h>

class Jdb_util
{
public:
  static bool is_mapped(void const *addr)
  {
    return Kmem::virt_to_phys(addr) != ~0UL;
  }
};

