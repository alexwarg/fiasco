#include "dbg_page_info.h"
#include "kmem_slab.h"

static Dbg_page_info_table _t;

Dbg_page_info_table &
Dbg_page_info::table()
{
  return _t;
}

static Kmem_slab_t<Dbg_page_info> _dbg_page_info_allocator("Dbg_page_info");

Dbg_page_info::Allocator *
Dbg_page_info::alloc()
{ return _dbg_page_info_allocator.slab(); }



