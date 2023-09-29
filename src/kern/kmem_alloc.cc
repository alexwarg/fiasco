
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
#include <warn.h>
#include <types.h>

static Kmem_alloc::Alloc _a;
Kmem_alloc::Alloc *Kmem_alloc::a = &_a;
unsigned long Kmem_alloc::_orig_free;
Kmem_alloc::Lock Kmem_alloc::lock;
Kmem_alloc *Kmem_alloc::_alloc;
Kmem_alloc_reaper::Reaper_list Kmem_alloc_reaper::mem_reapers;

static Static_object<Kmem_alloc> _kmem_alloc;

/**
 * Find a suitable "Kernel_tmp" KIP memory region with a minimal size.
 * Panic if no such region was found.
 */
static FIASCO_INIT
Mem_desc *find_kip_md_tmp_chunk(unsigned long size)
{
  for (auto &md: Kip::k()->mem_descs_a())
    if (md.type() == Mem_desc::Kernel_tmp && md.size() >= size)
      return &md;

  panic("Could not allocate buddy freemap.\n");
}

/**
 * Add a "Kernel_tmp" KIP memory region marked to the kernel memory except a
 * "remaining" part of size `skip` which shall be not considered, change the
 * descriptor type to "Reserved" and update _orig_free.
 */
FIASCO_INIT
void
Kmem_alloc::add_kip_md_tmp_to_kmem_sans_size(Mem_desc *md, unsigned long skip)
{
  if (md->size() > skip)
    {
      unsigned long add_sz = md->size() - skip;
      unsigned long md_kern = Mem_layout::phys_to_pmem(md->start()) + skip;
      a->add_mem((void*)md_kern, add_sz);

      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               md_kern, md->start(), add_sz);

      _orig_free += add_sz;
    }

  md->type(Mem_desc::Reserved);
}

/**
 * Add all "Kernel_tmp" KIP memory regions completely to the kernel memory,
 * change the descriptor types to "Reserved" and update _orig_free.
 */
FIASCO_INIT
void
Kmem_alloc::add_kip_md_tmp_to_kmem()
{
  for (auto &md: Kip::k()->mem_descs_a())
    if (md.type() == Mem_desc::Kernel_tmp)
      add_kip_md_tmp_to_kmem_sans_size(&md, 0);
}

FIASCO_INIT
void
Kmem_alloc::setup_kmem_from_kip_md_tmp(unsigned long freemap_size,
                                       unsigned long min_addr_kern)
{
  if (0)
    printf("Kmem_alloc: buddy freemap needs %lu bytes\n", freemap_size);

  Mem_desc *bmmd = find_kip_md_tmp_chunk(freemap_size);
  unsigned long bm_kern = Mem_layout::phys_to_pmem(bmmd->start());

  // Strictly speaking this is not necessary but it also doesn't make sense to
  // initialize the lower boundary of the kernel memory at the buddy freemap.
  if (bm_kern == min_addr_kern)
    min_addr_kern += freemap_size;

  if (0)
    printf("Kmem_alloc: allocator base = %014lx\n",
           Kmem_alloc::Alloc::calc_base_addr(min_addr_kern));

  a->init(min_addr_kern);
  a->setup_free_map(reinterpret_cast<unsigned long *>(bm_kern), freemap_size);

  // Add remaining the part of the KIP memory region containing the freemap but
  // omit the freemap itself.
  add_kip_md_tmp_to_kmem_sans_size(bmmd, freemap_size);

  // Add all other KIP memory regions marked as "Kernel_tmp" to kernel memory.
  add_kip_md_tmp_to_kmem();
}


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

  if (EXPECT_FALSE(!ret))
    WARNX(Error, "Out of memory requesting 0x%lx bytes)\n",
          cxx::int_value<Bytes>(size));

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

/**
 * Create map entries for all regions which could be used for kernel memory.
 *
 * This is actually the difference quantity of the conventional memory and all
 * unusable memory regions.
 *
 * \param kip        The KIP.
 * \param[out] map   The map containing the difference quantity of conventional
 *                   memory and unusable memory regions.
 * \param alignment  The required kernel memory alignment.
 * \returns  The amount of detected conventional memory in bytes. The amount of
 *           actually usable memory is smaller if any unusable region overlaps
 *           conventional memory.
 */
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

