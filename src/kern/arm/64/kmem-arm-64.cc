
#include <globalconfig.h>
#include <kmem.h>
#include <cassert>
#include "paging_bits.h"

#ifndef CONFIG_NONCONT_MEM

static
bool cont_mapped(Address phys_beg, Address phys_end, Address virt)
{
  for (Address p = phys_beg, v = virt;
       p < phys_end && v < Mem_layout::Registers_map_end;
       p += Config::SUPERPAGE_SIZE, v += Config::SUPERPAGE_SIZE)
    {
      auto e = Kmem::kdir->walk(Virt_addr(v), K_pte_ptr::Super_level);
      if (!e.is_valid() || p != e.page_addr())
        return false;
    }

  return true;
}

Address
Kmem::mmio_remap(Address phys, Address size)
{
  static Address ndev = 0;
  Address phys_page = Super_pg::trunc(phys);
  Address phys_end  = Super_pg::ceil(phys + size);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end; a += Config::SUPERPAGE_SIZE)
    {
      if (cont_mapped(phys_page, phys_end, a))
        return Super_pg::trunc(a) | Super_pg::offset(phys);
    }

  static_assert(Super_pg::aligned(Mem_layout::Registers_map_start),
                "Registers_map_start must be superpage-aligned");
  Address map_addr = Mem_layout::Registers_map_start + ndev;

  for (Address p = phys_page; p < phys_end; p+= Config::SUPERPAGE_SIZE)
    {
      Address dm = Mem_layout::Registers_map_start + ndev;
      assert(dm < Mem_layout::Registers_map_end);

      ndev += Config::SUPERPAGE_SIZE;

      auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
      assert (!m.is_valid());
      assert (m.page_order() == Config::SUPERPAGE_SHIFT);
      m.set_page(m.make_page(Phys_mem_addr(p),
                             Page::Attr(Page::Rights::RW(),
                                        Page::Type::Uncached(),
                                        Page::Kern::Global())));

      m.write_back_if(true, Mem_unit::Asid_kernel);
    }

  return map_addr | Super_pg::offset(phys);
}

#endif
