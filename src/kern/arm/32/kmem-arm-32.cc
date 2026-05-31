
#include <globalconfig.h>
#include <kmem.h>
#include <cassert>
#include <paging_bits.h>

#ifndef CONFIG_NONCONT_MEM

Address
Kmem::mmio_remap(Address phys, Address size)
{
  Address phys_page = Super_pg::trunc(phys);
  Address phys_end = Super_pg::ceil(phys + size);

  for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
    {
      auto m = kdir->walk(Virt_addr(p), K_pte_ptr::Super_level);
      if (m.is_valid())
        {
          assert (m.page_order() >= Config::SUPERPAGE_SHIFT);
          assert (m.page_addr() == cxx::mask_lsb(p, m.page_order()));
          assert (m.attribs().type == Page::Type::Uncached());
        }
      else
        {
          assert (m.page_order() == Config::SUPERPAGE_SHIFT);
          m.set_page(m.make_page(Phys_mem_addr(p),
                                 Page::Attr(Page::Rights::RW(),
                                            Page::Type::Uncached(),
                                            Page::Kern::Global())));

          m.write_back_if(true, Mem_unit::Asid_kernel);
        }
    }

  return phys;
}

#endif
