#pragma once

#include <mem_layout.h>
#include <globalconfig.h>

#ifdef CONFIG_NONCONT_MEM

#include <kmem_space.h>
#include <paging.h>
namespace Kmem_alloc_arch {
  inline Address to_phys(void *v)
  {
    return Mem_layout::kdir->virt_to_phys((Address)v);
  }
}
#else
namespace Kmem_alloc_arch {
  inline Address to_phys(void *v)
  {
    return (Address)v - Mem_layout::Map_base + Mem_layout::Sdram_phys_base;
  }
}

#endif

