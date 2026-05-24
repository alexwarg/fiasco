
#pragma once

#include "kmem_alloc.h"
#include "config.h"
#include "cpu.h"

inline ALWAYS_INLINE unsigned
Jdb_tbuf_init::max_size()
{ return 2 << 20; }

FIASCO_INIT
unsigned
Jdb_tbuf_init::allocate(unsigned size)
{
  _status = (Tracebuffer_status *)Kmem_alloc::allocator()
    ->alloc(Bytes(sizeof(Tracebuffer_status)));
  if (!_status)
    return 0;

  _buffer = (Tb_entry_union *)Kmem_alloc::allocator()
    ->alloc(Bytes(size));

  if (!_buffer)
    return 0;

  return size;
}

