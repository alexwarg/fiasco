
#pragma once

#include <panic.h>
#include "mem_layout.h"
#include "vmem_alloc.h"
#include "paging_bits.h"

inline ALWAYS_INLINE  unsigned
Jdb_tbuf_init::max_size()
{ return Mem_layout::Tbuf_buffer_size; }

FIASCO_INIT
unsigned
Jdb_tbuf_init::allocate(unsigned size)
{
  if (size > max_size())
    return max_size();

  _status = reinterpret_cast<Tracebuffer_status *>(Mem_layout::Tbuf_status_page);
  if (!Vmem_alloc::page_alloc(static_cast<void*>(status()), Vmem_alloc::ZERO_FILL))
    panic("jdb_tbuf: alloc status page at %p failed", static_cast<void*>(_status));

  _buffer = reinterpret_cast<Tb_entry_union *>(Mem_layout::Tbuf_buffer_area);
  Address va = reinterpret_cast<Address>(buffer());
  for (unsigned i = 0; i < Pg::count(size); ++i, va += Config::PAGE_SIZE)
    if (!Vmem_alloc::page_alloc(reinterpret_cast<void *>(va), Vmem_alloc::NO_ZERO_FILL))
      return Pg::size(i);

  return size;
}




