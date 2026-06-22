#pragma once

#include "task.h"

class Sigma0_task : public Task
{
public:
  explicit Sigma0_task(Ram_quota *q) : Task(q) {}
  bool is_sigma0() const override { return true; }
  Address virt_to_phys_s0(void *virt) const override
  { return reinterpret_cast<Address>(virt); }

  bool v_fabricate(Mem_space::Vaddr address,
                   Mem_space::Phys_addr *phys, Mem_space::Page_order *size,
                   Mem_space::Attr *attribs = nullptr) override
  {
    // special-cased because we don't do ptab lookup for sigma0
    *size = static_cast<Mem_space const &>(*this).largest_page_size();
    *phys = cxx::mask_lsb(Virt_addr(address), *size);

    if (attribs)
      *attribs = Mem_space::Attr(L4_fpage::Rights::URWX());

    return true;
  }

  Page_number mem_space_map_max_address() const override
  { return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

#if defined (CONFIG_IA32) || defined (CONFIG_AMD64)
  bool v_fabricate(Io_space::V_pfn address, Io_space::Phys_addr *phys,
                   Io_space::Page_order *size,
                   Io_space::Attr *attribs = nullptr) override
  {
    // special-cased because we don't do lookup for sigma0
    *size = Io_space::Page_order(Io_space::Map_superpage_shift);
    *phys = cxx::mask_lsb(address, *size);
    if (attribs)
      *attribs = L4_fpage::Rights::FULL();
    return true;
  }
#endif
};


