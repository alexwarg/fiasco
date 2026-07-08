#pragma once

#include <mem_layout.h>
#include <kmem-generic-api.h>
#include <paging.h>
#include <globalconfig.h>

class Kmem : public Mem_layout, public Kmem_generic_api
{
public:
  static bool is_io_bitmap_page_fault(Mword)
  { return false; }

  // currently a dummy the kernel runs unpaged
  static Pdir *const kdir;
};


