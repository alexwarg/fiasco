
#include <gic_mem.h>

#include <buddy_alloc.h>
#include <kmem.h>
#include <kmem_alloc.h>

#include <arithmetic.h>
#include <cstring>

/**
 * Allocate uninitialized memory with alignment requirements.
 */
Gic_mem::Mem_chunk
Gic_mem::alloc_mem(unsigned size, unsigned align)
{
  assert(size >= Kmem_alloc::Alloc::Min_size);

  // Underlying buddy allocator can only provide naturally aligned blocks of
  // power-of-two in size. Thus, to ensure proper alignment, we may have to
  // allocate more memory than requested.
  if (cxx::log2u(size) < cxx::log2u(align))
    size = align;

  void *mem = Kmem_alloc::allocator()->alloc(Bytes(size));
  assert(reinterpret_cast<Address>(mem) % align == 0);
  return Mem_chunk(mem, size);
}

/**
 * Allocate zero initialized memory with alignment requirements.
 */
Gic_mem::Mem_chunk
Gic_mem::alloc_zmem(unsigned size, unsigned align)
{
  Gic_mem::Mem_chunk mem = alloc_mem(size, align);
  if (mem.is_valid())
    memset(mem.virt_ptr(), 0, size);
  return mem;
}

void
Gic_mem::free_mem(void *mem, unsigned size)
{
  Kmem_alloc::allocator()->free(Bytes(size), mem);
}

Address
Gic_mem::to_phys(Address virt)
{
  return Kmem::kdir->virt_to_phys(virt);
}
