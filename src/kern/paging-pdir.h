#pragma once

#include <ptab_base.h>
#include <mem_layout.h>

template<typename PTE_PTR, typename TRAITS, typename VA>
class Pdir_t : public Ptab::Base<PTE_PTR, TRAITS, VA, Mem_layout>
{
public:
  static constexpr Ptab::Level_id Super_level = PTE_PTR::Super_level;

  Address
  virt_to_phys(Address virt) const
  {
    Virt_addr va(virt);
    auto i = this->walk(va);
    if (!i.is_valid())
      return ~0;

    return i.page_addr() | cxx::get_lsb(virt, i.page_order());
  }
};


