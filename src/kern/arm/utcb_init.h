#pragma once

#include <globalconfig.h>
#ifdef CONFIG_ARM_V5
#include <panic.h>
#include <vmem_alloc.h>
#include <mem_layout.h>
namespace Utcb_init
{
  inline void init()
  {
    if (!Vmem_alloc::page_alloc((void *)Mem_layout::Utcb_ptr_page,
                                Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
      panic("UTCB pointer page allocation failure");
  }
}
#else
namespace Utcb_init
{
  inline void init()
  {}
}
#endif

