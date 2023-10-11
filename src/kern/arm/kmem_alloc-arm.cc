
#include <kmem_alloc.h>
// Kmem_alloc::Kmem_alloc() puts those Mem_region_map's on the stack which
// is slightly larger than our warning limit but it's on the boot stack only
// so this it is ok.
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include <mem_unit.h>
#include <mem_region.h>
#include <ram_quota.h>
#include <kip.h>
#include <buddy_alloc.h>
#include <paging_bits.h>

#include <cstdio>
#include <panic.h>

#include <globalconfig.h>

#ifdef CONFIG_NONCONT_MEM

#include <mem_layout.h>
#include <kmem_space.h>
#include <paging.h>
#include <kmem.h>

static bool
map_pmem(unsigned long phys, unsigned long size)
{
  static unsigned long next_map = Mem_layout::Pmem_start;
  size = Super_pg::ceil(size + Super_pg::offset(phys));
  phys = Super_pg::trunc(phys);

  if (next_map + size > Mem_layout::Pmem_end)
    return false;

  for (unsigned long i = 0; i <size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = Kmem::kdir->walk(Virt_addr(next_map + i), Kpdir::Super_level);
      pte.set_page(Phys_mem_addr(phys + i),
                   Page::Attr(Page::Rights::RW()));
      pte.write_back_if(true, Mem_unit::Asid_kernel);
    }
  Mem_layout::add_pmem(phys, next_map, size);
  next_map += size;
  return true;
}

static unsigned long _freemap[
  Kmem_alloc::Alloc::free_map_bytes(Mem_layout::Map_base, Mem_layout::Pmem_end - 1)
  / sizeof(unsigned long)];

Kmem_alloc::Kmem_alloc()
{
  // The -Wframe-larger-than= warning for this function is known and
  // no problem, because the function runs only on our boot stack.
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)", available_size);

  Mem_region last = map[map.length() - 1];
  if (last.end - Mem_layout::Sdram_phys_base < Config::kernel_mem_max)
    a->init(Mem_layout::Map_base);
  else
    a->init(Mem_layout::Pmem_start);

  a->setup_free_map(_freemap, Kmem_alloc::Alloc::free_map_bytes(
    Mem_layout::Map_base, Mem_layout::Pmem_end - 1));

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
	f.start += (f.size() - alloc_size);

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));
      if (0)
	printf("Kmem_alloc: [%08lx; %08lx] sz=%ld\n", f.start, f.end, f.size());
      if (Mem_layout::phys_to_pmem(f.start) == ~0UL)
	if (!map_pmem(f.start, f.size()))
        panic("Kmem_alloc: cannot map heap memory [%08lx; %08lx]",
              f.start, f.end);

      a->add_mem((void *)Mem_layout::phys_to_pmem(f.start), f.size());
      alloc_size -= f.size();
    }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory (missing %ld)",
          alloc_size);
}

#else // CONFIG_NONCONT_MEM

Kmem_alloc::Kmem_alloc()
{
  // The -Wframe-larger-than= warning for this function is known and
  // no problem, because the function runs only on our boot stack.
  Mword offset = Mem_layout::Map_base - Mem_layout::Sdram_phys_base;
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)", available_size);

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region &f = map[i];
      if (f.size() > alloc_size)
        f.start += (f.size() - alloc_size);

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));

      alloc_size -= f.size();
      if (!alloc_size)
        {
          // remove all the unsed regions
          map.del(0, i);
          break;
        }
    }

  unsigned long freemap_size
    = Alloc::free_map_bytes(map[0].start, map[map.length()-1].end);

  unsigned long freemap_addr = ~0UL;

  for (int i = map.length() - 1; i >= 0; --i)
    {
      Mem_region &f = map[i];
      if (f.size() >= freemap_size)
        {
          freemap_addr = f.end - freemap_size + 1 + offset;
          f.end -= freemap_size;
          break;
        }
    }

  if (freemap_addr == ~0UL)
    panic("could not allocate freemap for buddy allocator");

  a->init(map[0].start + offset);
  a->setup_free_map((unsigned long *)freemap_addr, freemap_size);

  for (int i = map.length() - 1; i >= 0; --i)
    {
      Mem_region f = map[i];
      a->add_mem((void *)(f.start + offset), f.size());
    }
}

#endif


void Kmem_alloc::debug_dump() const
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %ldKB out of %dKB of Kmem\n",
	 (Config::KMEM_SIZE - free + 1023)/1024,
	 (Config::KMEM_SIZE        + 1023)/1024);
}
