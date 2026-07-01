#include <vmem_alloc.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include <config.h>
#include <kdb_ke.h>
#include <kmem.h>
#include <kmem_alloc.h>
#include <mem_layout.h>
#include <mem_unit.h>
#include <paging.h>
#include <space.h>

void*
Vmem_alloc::page_alloc(void *address, Zero_fill zf, unsigned mode)
{
  void *vpage = 0;
  Address page;

  vpage = Kmem_alloc::allocator()->alloc(Config::page_order());

  if (EXPECT_FALSE(!vpage))
    return 0;

  // insert page into master page table
  auto e = Kmem::kdir->walk(Virt_addr(address), Pdir::leaf_level(),
                            false, pdir_alloc(Kmem_alloc::allocator()));
  if (EXPECT_FALSE(e.is_valid()))
    {
      kdb_ke("page_alloc: address already mapped");
      goto error;
    }

  if (e.level != Pdir::leaf_level())
    goto error;

  if (zf == ZERO_FILL)
    memset(vpage, 0, Config::PAGE_SIZE);

  page = Mem_layout::pmem_to_phys((Address)vpage);

  e.set_page(page, Pt_entry::Writable | Pt_entry::Dirty
                   | Pt_entry::Referenced
                   | Pt_entry::global() | (mode & User ? (unsigned)Pt_entry::User : 0));
  return address;

error:
  Kmem_alloc::allocator()->free(Config::page_order(), vpage); // 2^0 = 1 page
  return 0;
}

