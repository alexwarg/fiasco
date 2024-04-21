#include <space-arch.h>
#include <mem.h>
#include <config.h>
#include <kmem_alloc.h>

void
Space_ia32_ldt_base::Ldt::alloc()
{
  // LDT maximum size is one page
  _addr = Kmem_alloc::allocator()->alloc(Config::page_order());
  Mem::memset_mwords(reinterpret_cast<void *>(addr()), 0,
                     Config::PAGE_SIZE / sizeof(Mword));
}

Space_ia32_ldt_base::Ldt::~Ldt()
{
  if (addr())
    Kmem_alloc::allocator()->free(Config::page_order(),
                                  reinterpret_cast<void*>(addr()));
}


