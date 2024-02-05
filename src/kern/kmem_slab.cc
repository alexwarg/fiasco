#include "kmem_slab.h"
#include "kmem_alloc.h"

#include <cassert>

typedef cxx::S_list_bss<Kmem_slab> Reap_list;
static Reap_list reap_list;

// Specializations providing their own block_alloc()/block_free() can
// also request slab sizes larger than one page.
Kmem_slab::Kmem_slab(unsigned long slab_size,
				   unsigned elem_size,
				   unsigned alignment,
				   char const *name)
  : Slab_cache(slab_size, elem_size, alignment, name)
{
  reap_list.atomic_add(this);
}

// Specializations providing their own block_alloc()/block_free() can
// also request slab sizes larger than one page.
Kmem_slab::Kmem_slab(unsigned elem_size,
                     unsigned alignment,
                     char const *name,
                     unsigned long min_size,
                     unsigned long max_size)
  : Slab_cache(elem_size, alignment, name, min_size, max_size)
{
  reap_list.atomic_add(this);
}



// Callback functions called by our super class, Slab_cache, to
// allocate or free blocks
void *
Kmem_slab::block_alloc(unsigned long size, unsigned long)
{
  static_cast<void>(size);
  assert (size >= Buddy_alloc::Min_size);
  assert (size <= Buddy_alloc::Max_size);
  assert (!(size & (size - 1)));
  return Kmem_alloc::allocator()->alloc(Bytes(size));
}

void
Kmem_slab::block_free(void *block, unsigned long size)
{
  Kmem_alloc::allocator()->free(Bytes(size), block);
}

// 
// Memory reaper
// 
size_t
Kmem_slab::reap_all(bool desperate)
{
  size_t freed = 0;

  for (auto *alloc: reap_list)
    {
      size_t got;
      do
	{
	  got = alloc->reap();
	  freed += got;
	}
      while (desperate && got);
    }

  return freed;
}

static Kmem_alloc_reaper kmem_slab_reaper(Kmem_slab::reap_all);

#if defined (CONFIG_JDB)

cxx::S_list<Kmem_slab> const &
jdb_kmem_slab_list();

cxx::S_list<Kmem_slab> const &
jdb_kmem_slab_list()
{
  return reap_list;
}

#endif

