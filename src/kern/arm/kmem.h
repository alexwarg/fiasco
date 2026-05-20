#pragma once

#include <mem_layout.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <paging.h>
#include <config.h>

class Kmem : public Mem_layout
{
public:
  static Mword is_kmem_page_fault(Mword pfa, Mword error)
  {
    if (IS_ENABLED(CONFIG_CPU_VIRT) && !PF::is_usermode_error(error))
      return true;

    return in_kernel(pfa);
  }

  static Mword is_io_bitmap_page_fault(Mword /*pfa*/)
  {
    return 0;
  }

  static Address mmio_remap(Address phys, Address size);
};


