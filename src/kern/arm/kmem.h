#pragma once

#include <mem_layout.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <paging.h>
#include <config.h>

class Kmem : public Mem_layout
{
public:
  static Kpdir *kdir;

  static bool is_kmem_page_fault(Mword pfa, Mword error)
  {
    if (IS_ENABLED(CONFIG_CPU_VIRT) && !PF::is_usermode_error(error))
      return true;

    return in_kernel(pfa);
  }

  static bool is_io_bitmap_page_fault(Mword /*pfa*/)
  {
    return false;
  }

  static Address mmio_remap(Address phys, Address size);
};


