#pragma once

#include "kmem.h"

class Jdb_util
{
public:
  static bool is_mapped(void const *addr)
  {
    return Kmem::kdir->virt_to_phys((Address)addr) != Address(~0UL);
  }
};
