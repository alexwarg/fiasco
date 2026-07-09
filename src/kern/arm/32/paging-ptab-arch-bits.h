#pragma once

#include <ptab_base.h>
#include <paging-pdir.h>
#include <globalconfig.h>

#ifdef CONFIG_ARM_LPAE

using Ptab_traits =
  Ptab::List< Ptab::Traits< Unsigned64, 30, 2, true>,
              Ptab::Traits< Unsigned64, 21, 9, true>,
              Ptab::Traits< Unsigned64, 12, 9, true> >;


#else

using Ptab_traits =
  Ptab::List< Ptab::Traits< Unsigned32, 20, 12, true>,
              Ptab::Traits< Unsigned32, 12, 8, true> >;

#endif

static constexpr Ptab::Level_id K_ptab_super_level {1};
static constexpr Ptab::Level_id Ptab_super_level {1};

using Ptab_traits_vpn = Ptab::Shift<Ptab_traits, Virt_addr::Shift>;
using Ptab_va_vpn = Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift>;
using K_ptab_traits_vpn = Ptab_traits_vpn;

template<typename PTE_PTR>
using U_pdir_t = Pdir_x_t<PTE_PTR, Ptab_va_vpn, Ptab::Base, Ptab_traits_vpn>;

