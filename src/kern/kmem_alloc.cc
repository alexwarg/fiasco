
#include <kmem_alloc.h>

#include <auto_quota.h>
#include <cxx/slist>

#include <spin_lock.h>
#include <lock_guard.h>
#include <initcalls.h>

#include <cassert>
#include <cxx/atomic>

#include <config.h>
#include <kip.h>
#include <mem_layout.h>
#include <mem_region.h>
#include <buddy_alloc.h>
#include <panic.h>
#include <types.h>

static Kmem_alloc::Alloc _a;
Kmem_alloc::Alloc *Kmem_alloc::a = &_a;
unsigned long Kmem_alloc::_orig_free;
Kmem_alloc::Lock Kmem_alloc::lock;
Kmem_alloc *Kmem_alloc::_alloc;
Kmem_alloc_reaper::Reaper_list Kmem_alloc_reaper::mem_reapers;

static Static_object<Kmem_alloc> _kmem_alloc;

FIASCO_INIT
void
Kmem_alloc::init()
{
  Kmem_alloc::allocator(_kmem_alloc.construct());
}

void *
Kmem_alloc::alloc(Bytes size)
{
  const size_t sz = cxx::int_value<Bytes>(size);
  assert(sz >= 8 /* NEW INTERFACE PARANOIA */);
  void* ret;

  {
    auto guard = lock_guard(lock);
    ret = a->alloc(sz);
  }

  if (!ret)
    {
      Kmem_alloc_reaper::morecore (/* desperate= */ true);

      auto guard = lock_guard(lock);
      ret = a->alloc(sz);
    }

  return ret;
}

void
Kmem_alloc::free(Bytes size, void *page)
{
  const size_t sz = cxx::int_value<Bytes>(size);
  assert(sz >= 8 /* NEW INTERFACE PARANOIA */);
  auto guard = lock_guard(lock);
  a->free(page, sz);
}

void
Kmem_alloc::dump() const
{
  a->dump();
}

FIASCO_INIT
unsigned long
Kmem_alloc::create_free_map(Kip const *kip, Mem_region_map_base *map)
{
  unsigned long available_size = 0;

  for (auto const &md: kip->mem_descs_a())
    {
      if (!md.valid())
        {
          const_cast<Mem_desc &>(md).type(Mem_desc::Undefined);
          continue;
        }

      if (md.is_virtual())
        continue;

      unsigned long s = md.start();
      unsigned long e = md.end();

      // Sweep out stupid descriptors (that have the end before the start)

      switch (md.type())
        {
        case Mem_desc::Conventional:
          s = (s + Config::PAGE_SIZE - 1) & ~(Config::PAGE_SIZE - 1);
          e = ((e + 1) & ~(Config::PAGE_SIZE - 1)) - 1;
          if (e <= s)
            break;
          available_size += e - s + 1;
          if (!map->add(Mem_region(s, e)))
            panic("Kmem_alloc::create_free_map(): memory map too small");
          break;
        case Mem_desc::Reserved:
        case Mem_desc::Dedicated:
        case Mem_desc::Shared:
        case Mem_desc::Arch:
        case Mem_desc::Bootloader:
          s = s & ~(Config::PAGE_SIZE - 1);
          e = ((e + Config::PAGE_SIZE) & ~(Config::PAGE_SIZE - 1)) - 1;
          if (!map->sub(Mem_region(s, e)))
            panic("Kmem_alloc::create_free_map(): memory map too small");
          break;
        default:
          break;
        }
    }

  return available_size;
}

