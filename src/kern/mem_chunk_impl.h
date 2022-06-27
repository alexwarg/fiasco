
#pragma once

#include <mem_chunk.h>
#include <buddy_alloc.h>
#include <kmem_alloc.h>
#include <arithmetic.h>
#include <cstring>

/**
 * Allocate uninitialized memory with alignment requirements.
 *
 * \param size   Size of the memory to allocate in bytes.
 * \param align  Alignment requirement of the memory to allocate, must be a
 *               power of two. The default alignment is defined as `size`
 *               rounded to next largest power of two.
 */
template<typename MEM>
MEM
Mem_chunk::alloc_mem(unsigned size, unsigned align)
{
  assert(size >= Kmem_alloc::Alloc::Min_size);
  assert(align == (1u << cxx::log2u(align)));

  // Underlying buddy allocator can only provide naturally aligned blocks of
  // power-of-two in size. Thus, to ensure proper alignment, we may have to
  // allocate more memory than requested.
  unsigned alloc_size = max(size, align);
  void *mem = Kmem_alloc::allocator()->alloc(Bytes(alloc_size));
  assert(reinterpret_cast<Address>(mem) % align == 0);
  return MEM(mem, size, alloc_size);
}

/**
 * Allocate zero initialized memory with alignment requirements.
 *
 * \param size   Size of the memory to allocate in bytes.
 * \param align  Alignment requirement of the memory to allocate, must be a
 *               power of two. The default alignment is defined as `size`
 *               rounded to next largest power of two.
 */
template<typename MEM>
MEM
Mem_chunk::alloc_zmem(unsigned size, unsigned align)
{
  MEM mem = alloc_mem<MEM>(size, align);
  if (mem.is_valid())
    memset(mem.virt_ptr(), 0, size);
  return mem;
}


