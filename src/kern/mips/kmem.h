#pragma once

#include <mem_layout.h>
#include <kmem-generic-api.h>
#include <paging.h>
#include <globalconfig.h>

class Kmem : public Mem_layout, public Kmem_generic_api
{
public:
#ifdef CONFIG_BIT32
  static bool is_kmem_page_fault(Mword pfa, Mword /*cause*/)
  { return pfa >= 0x80000000; }
#endif
#ifdef CONFIG_BIT64
  static bool is_kmem_page_fault(Mword pfa, Mword /*cause*/)
  { return pfa >= 0x8000000000000000; }
#endif

  static bool is_io_bitmap_page_fault(Mword)
  { return false; }

  // currently a dummy the kernel runs unpaged
  static Pdir *const kdir;
};


