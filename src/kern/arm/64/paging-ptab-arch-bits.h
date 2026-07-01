#pragma once

#include <ptab_base.h>
#include <globalconfig.h>

using K_ptab_traits =
  Ptab::List< Ptab::Traits< Unsigned64, 39, 9, false>,
              Ptab::Traits< Unsigned64, 30, 9, true>,
              Ptab::Traits< Unsigned64, 21, 9, true>,
              Ptab::Traits< Unsigned64, 12, 9, true> >;

using K_ptab_traits_vpn = Ptab::Shift<K_ptab_traits, Virt_addr::Shift>;

static constexpr Ptab::Level_id K_ptab_super_level {1};

/* 4-levels for stage 2 paging with a maximum IPA size of 48bits */
using Ptab_traits_4lvl =
  Ptab::List< Ptab::Traits< Unsigned64, 39, 9, false>,
              Ptab::Traits< Unsigned64, 30, 9, true>,
              Ptab::Traits< Unsigned64, 21, 9, true>,
              Ptab::Traits< Unsigned64, 12, 9, true> >;

/* 3-levels for stage 2 paging with a maximum IPA size of 40bits */
using Ptab_traits_3lvl =
  Ptab::List< Ptab::Traits< Unsigned64, 30, 10, true>,
              Ptab::Traits< Unsigned64, 21, 9, true>,
              Ptab::Traits< Unsigned64, 12, 9, true> >;

using Ptab_traits_vpn_3lvl = Ptab::Shift<Ptab_traits_3lvl, Virt_addr::Shift>;
using Ptab_traits_vpn_4lvl = Ptab::Shift<Ptab_traits_4lvl, Virt_addr::Shift>;
using Ptab_va_vpn = Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift>;
#ifdef CONFIG_ARM_PT48
using Ptab_traits_vpn = Ptab_traits_vpn_4lvl;
#else
using Ptab_traits_vpn = Ptab_traits_vpn_3lvl;
#endif

static constexpr Ptab::Level_id Ptab_super_level {1};

