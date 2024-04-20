#pragma once

#include <ptab_base.h>
#include <globalconfig.h>

using K_ptab_traits =
  Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
               Ptab::Traits< Unsigned64, 30, 9, true>,
               Ptab::Traits< Unsigned64, 21, 9, true>,
               Ptab::Traits< Unsigned64, 12, 9, true> >::List;

using K_ptab_traits_vpn = Ptab::Shift<K_ptab_traits, Virt_addr::Shift>::List;

static constexpr unsigned K_ptab_max_level = Ptab::Level<K_ptab_traits>::Max_level;
static constexpr unsigned K_ptab_super_level = K_ptab_max_level - 1;

#ifdef CONFIG_ARM_PT48

using Ptab_traits =
  Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
               Ptab::Traits< Unsigned64, 30, 9, true>,
               Ptab::Traits< Unsigned64, 21, 9, true>,
               Ptab::Traits< Unsigned64, 12, 9, true> >::List;

#else

/* 3-levels for stage 2 paging with a fixed IPA size of 40bits */
using Ptab_traits =
  Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 10, true>,
               Ptab::Traits< Unsigned64, 21, 9, true>,
               Ptab::Traits< Unsigned64, 12, 9, true> >::List;

#endif

using Ptab_traits_vpn = Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List;
using Ptab_va_vpn = Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift>;

static constexpr unsigned Ptab_max_level = Ptab::Level<Ptab_traits>::Max_level;
static constexpr unsigned Ptab_super_level = Ptab_max_level - 1;

