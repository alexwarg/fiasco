#pragma once

#include <mem_layout.h>
#include <globalconfig.h>

#include <kmem_space.h>
#include <kmem.h>
#include <paging.h>

namespace Kmem_alloc_arch {
  inline Address to_phys(void *v)
  {
    return Mem_layout::pmem_to_phys((Address)v);
  }
}

