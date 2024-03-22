
#include "io_space.h"

#include "types.h"
#include "mem_space.h"

#include "config.h"
#include "l4_types.h"
#include "kmem_alloc.h"
#include "paging.h"

#include <cassert>
#include <cstring>
#include <cxx/atomic>

void
Generic_io_space_base::free_memory(Mem_space *m, Ram_quota *q)
{
  auto dir = m->dir();
  if (!dir)
    return;

  auto iopte = dir->walk(Virt_addr(Mem_layout::Io_bitmap));

  // do we have an IO bitmap?
  if (iopte.is_valid())
    {
      // sanity check
      assert (iopte.level != Pdir::Super_level);

      Kmem_alloc::allocator()->q_free_phys(q, Config::page_order(),
                                           iopte.page_addr());

      // switch to next page-table entry
      ++iopte;

      if (iopte.is_valid())
        Kmem_alloc::allocator()->q_free_phys(q, Config::page_order(),
                                             iopte.page_addr());

      auto iopde = dir->walk(Virt_addr(Mem_layout::Io_bitmap),
                             Pdir::Super_level);

      // free the page table
      Kmem_alloc::allocator()->q_free_phys(q, Config::page_order(),
                                           iopde.next_level());

      // free reference
      *iopde.pte = 0;
    }
}

//
// Utilities for map<Generic_io_space> and unmap<Generic_io_space>
//

/** Enable one IO port in the IO space.
    This function is called in the context of the IPC sender!
    @param port_number address of the port
    @return Insert_warn_exists if some ports were mapped in that IO page
       Insert_err_nomem if memory allocation failed
       Insert_ok if otherwise insertion succeeded
 */
Generic_io_space_base::Status
Generic_io_space_base::io_insert(Mem_space *m, Ram_quota *q, Address port_number)
{
  assert(port_number < Mem_layout::Io_port_max);

  Address port_virt = Mem_layout::Io_bitmap + (port_number >> 3);
  Address port_phys = m->virt_to_phys(port_virt);

  if (port_phys == ~0UL)
    {
      // nothing mapped! Get a page and map it in the IO bitmap
      void *page;
      if (!(page = Kmem_alloc::allocator()->q_alloc(q, Config::page_order())))
	return Insert_err_nomem;

      // clear all IO ports
      // bit == 1 disables the port
      // bit == 0 enables the port
      memset(page, 0xff, Config::PAGE_SIZE);

      Mem_space::Status status =
	m->v_insert(
	    Mem_space::Phys_addr(Mem_layout::pmem_to_phys(page)),
	    Virt_addr(port_virt & Config::PAGE_MASK),
	    Mem_space::Page_order(Config::PAGE_SHIFT),
            Mem_space::Attr(L4_fpage::Rights::RW()));

      if (status == Mem_space::Insert_err_nomem)
	{
	  Kmem_alloc::allocator()->free(Config::page_order(), page);
	  q->free(Config::PAGE_SIZE);
	  return Insert_err_nomem;
	}

      // we've been careful, so insertion should have succeeded
      assert(status == Mem_space::Insert_ok);

      port_phys = m->virt_to_phys(port_virt);
      assert(port_phys != ~0UL);
    }

  // so there is memory mapped in the IO bitmap -- write the bits now
  Unsigned8 *port = static_cast<Unsigned8 *> (Kmem::phys_to_virt(port_phys));

  if (*port & get_port_bit(port_number)) // port disabled?
    {
      *port &= ~get_port_bit(port_number);
      addto_io_counter(1);
      return Insert_ok;
    }

  // already enabled
  return Insert_warn_exists;
}


