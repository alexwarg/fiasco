#pragma once

#include <ptab_base.h>
#include <globalconfig.h>
#include <paging-pdir.h>

#include <cpu.h>
#include <ptab_base-multi.h>
#include <ptab_base-iterative.h>

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
using Ptab_traits_vpn = Ptab_traits_vpn_4lvl;
static constexpr Ptab::Level_id Ptab_super_level {1};

#ifdef CONFIG_ARM_SINGLE_PT
#  ifdef CONFIG_ARM_PT40_ONLY
template<typename PTE_PTR>
using U_pdir_t = Pdir_x_t<PTE_PTR, Ptab_va_vpn, Ptab::Iterative_base, Ptab_traits_vpn_3lvl>;
#  endif
#  ifdef CONFIG_ARM_PT48_ONLY
template<typename PTE_PTR>
using U_pdir_t = Pdir_x_t<PTE_PTR, Ptab_va_vpn, Ptab::Iterative_base, Ptab_traits_vpn_4lvl>;
#  endif
#else

struct Pdir_selector : Alternative_static_functor<Pdir_selector>
{
  static bool probe() { return Cpu::pa_range() >= 4; }
  static unsigned select() { return Pdir_selector() ? 1 : 0; }
};

template<typename PTE_PTR>
using U_pdir_t = Pdir_x_t<PTE_PTR, Ptab_va_vpn, Ptab::Multi_base, Ptab_traits_vpn_4lvl,
                          Pdir_selector,
                          Ptab_traits_vpn_3lvl, Ptab_traits_vpn_4lvl>;

#endif


