#pragma once

#include <mem_layout.h>

namespace Kmem_alloc_arch {
  inline Phys_mem_addr::Value to_phys(void *v)
  {
    return Mem_layout::pmem_to_phys(v);
  }
}

