#pragma once

#include <ptab_base.h>
#include <globalconfig.h>

#ifdef CONFIG_ARM_LPAE

using Ptab_traits =
  Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 2, true>,
               Ptab::Traits< Unsigned64, 21, 9, true>,
               Ptab::Traits< Unsigned64, 12, 9, true> >::List;


#else

using Ptab_traits =
  Ptab::List< Ptab::Traits< Unsigned32, 20, 12, true>,
              Ptab::Traits< Unsigned32, 12, 8, true> >;

#endif

static constexpr unsigned K_ptab_max_level = Ptab::Level<Ptab_traits>::Max_level;
static constexpr unsigned K_ptab_super_level = K_ptab_max_level - 1;
static constexpr unsigned Ptab_max_level = Ptab::Level<Ptab_traits>::Max_level;
static constexpr unsigned Ptab_super_level = Ptab_max_level - 1;

using Ptab_traits_vpn = Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List;
using Ptab_va_vpn = Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift>;
using K_ptab_traits_vpn = Ptab_traits_vpn;
