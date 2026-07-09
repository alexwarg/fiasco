#pragma once

#include <ptab_base.h>
#include <mem_layout.h>

template<typename PTE_PTR, typename VA,
         template<typename ...> class BASE,
         typename ...TRAITS>
class Pdir_x_t : public BASE<PTE_PTR, VA, Mem_layout, TRAITS...>
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


template<typename PTE_PTR, typename TRAITS, typename VA,
         template<typename ...> class BASE = Ptab::Base>
using Pdir_t = Pdir_x_t<PTE_PTR, VA, BASE, TRAITS>;
