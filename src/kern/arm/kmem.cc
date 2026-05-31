
#include <globalconfig.h>
#include <kmem.h>
#include <cassert>
#include <paging_bits.h>

#ifdef CONFIG_NONCONT_MEM

Address
Kmem::mmio_remap(Address phys, Address size)
{
  static Address ndev = 0;

  Address phys_page = Super_pg::trunc(phys);
  Address phys_end = Super_pg::ceil(phys + size);
  bool needs_remap = false;

  for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
    {
      if (phys_to_pmem(p) == ~0UL)
        {
          needs_remap = true;
          break;
        }
    }

  if (needs_remap)
    {
      for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
        {
          Address dm = Mem_layout::Registers_map_start + ndev;
          assert(dm < Mem_layout::Registers_map_end);

          ndev += Config::SUPERPAGE_SIZE;

          auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
          assert (!m.is_valid());
          assert (m.page_order() == Config::SUPERPAGE_SHIFT);
          m.set_page(m.make_page(Phys_mem_addr(p),
                                 Page::Attr(Page::Rights::RWX(),
                                            Page::Type::Uncached(),
                                            Page::Kern::Global())));

          m.write_back_if(true, Mem_unit::Asid_kernel);
          add_pmem(p, dm, Config::SUPERPAGE_SIZE);
        }
    }

  return phys_to_pmem(phys);
}

#endif

