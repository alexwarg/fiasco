
#include <mem_chunk.h>

#include <mem_layout.h>
#include <kmem_alloc.h>

void
Mem_chunk::free_mem(void *mem, unsigned size)
{
  Kmem_alloc::allocator()->free(Bytes(size), mem);
}

Address
Mem_chunk::to_phys(Address virt)
{
  return Mem_layout::pmem_to_phys(virt);
}

