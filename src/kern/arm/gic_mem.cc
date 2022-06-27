
#include <gic_mem.h>

#include <buddy_alloc.h>
#include <kmem.h>
#include <kmem_alloc.h>
#include <mem_chunk_impl.h>

#include <arithmetic.h>
#include <cstring>

/**
 * Allocate uninitialized memory with alignment requirements.
 */
Gic_mem
Gic_mem::alloc_mem(unsigned size, unsigned align)
{ return Mem_chunk::alloc_mem<Gic_mem>(size, align); }

/**
 * Allocate zero initialized memory with alignment requirements.
 */
Gic_mem
Gic_mem::alloc_zmem(unsigned size, unsigned align)
{ return Mem_chunk::alloc_zmem<Gic_mem>(size, align); }
