
#include "boot_alloc.h"

#include "kmem_alloc.h"
#include "warn.h"

#include <cstdio>
#include <cstring>

Boot_alloced::Block_list Boot_alloced::_free;

void *
Boot_alloced::alloc(size_t size)
{
  if (Debug_boot_alloc)
    printf("Boot_alloc: size=0x%zx\n", size);

  // this is best fit list-based allocation

  Block_list::Iterator best = _free.end();
  for (Block_list::Iterator curr = _free.begin(); curr != _free.end(); ++curr)
    {
      if (((best == _free.end()) || curr->size < best->size)
          && curr->size >= size)
        best = curr;
    }

  if (best == _free.end())
    {
      // start from 1k
      unsigned long alloc_size = 1024;

      // look for a size suitable and buddy friendly
      while (alloc_size < size)
        alloc_size <<= 1;

      Block *b =
        static_cast<Block*>(Kmem_alloc::allocator()->alloc(Bytes(alloc_size)));
      if (Debug_boot_alloc)
        printf("Boot_alloc: allocated extra memory block @%p (size=%lx)\n",
               static_cast<void *>(b), alloc_size);

      if (!b)
        return nullptr;

      b->size = alloc_size;
      _free.add(b);
      best = _free.begin();
    }


  void *b = *best;
  Address b_addr = reinterpret_cast<Address>(b);
  Address rem_addr = (b_addr + size + sizeof(Block) - 1) & ~(sizeof(Block) - 1);
  long rem_sz = b_addr + (*best)->size - rem_addr;
  if (Debug_boot_alloc)
    printf("Boot_alloc: @ %p\n", b);
  if (rem_sz > static_cast<long>(sizeof(Block)))
    {
      Block *rem = reinterpret_cast<Block *>(rem_addr);
      rem->size = rem_sz;
      _free.replace(best, rem);
      if (Debug_boot_alloc)
        printf("Boot_alloc: remaining free block @ %p (size=%lx)\n",
               static_cast<void *>(rem), rem_sz);
    }
  else
    _free.erase(best);

  memset(b, 0, size);
  return b;
}

void
Boot_alloced::operator delete (void *b)
{
  WARN("Boot_alloc: trying to delete boot-time allocated object @ %p\n", b);
}

void
Boot_alloced::operator delete [] (void *b)
{
  WARN("Boot_alloc: trying to delete boot-time allocated object @ %p\n", b);
}

