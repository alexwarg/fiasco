
#include <mem_chunk.h>

#include <kmem.h>
#include <kmem_alloc.h>

void
Mem_chunk::free_mem(void *mem, unsigned size)
{
  Kmem_alloc::allocator()->free(Bytes(size), mem);
}

Address
Mem_chunk::to_phys(Address virt)
{
  return Kmem::kdir->virt_to_phys(virt);
}

