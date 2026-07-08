#pragma once

#include <mem_layout.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <paging.h>
#include <config.h>
#include <kmem-generic-api.h>

class Kmem : public Mem_layout, public Kmem_generic_api
{
public:
  static Kpdir *kdir;

  static bool is_io_bitmap_page_fault(Mword /*pfa*/)
  {
    return false;
  }
};


