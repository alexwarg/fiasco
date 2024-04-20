#pragma once

#include <types.h>
#include <ptab_base.h>

using Ptab_traits =
  Ptab::List< Ptab::Traits<Unsigned32, 22, 10, true, false>,
              Ptab::Traits<Unsigned32, 12, 10, true> >;

using Ptab_traits_vpn = Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List;
using Ptab_va_vpn = Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift>;

class Pt_entry_bits
{
public:
  enum
  {
    Super_level   = 0,
    Valid         = 0x00000001, ///< Valid
    Writable      = 0x00000002, ///< Writable
    User          = 0x00000004, ///< User accessible
    Write_through = 0x00000008, ///< Write through
    Cacheable     = 0x00000000, ///< Cache is enabled
    Noncacheable  = 0x00000010, ///< Caching is off
    Referenced    = 0x00000020, ///< Page was referenced
    Dirty         = 0x00000040, ///< Page was modified
    Pse_bit       = 0x00000080, ///< Indicates a super page
    Cpu_global    = 0x00000100, ///< pinned in the TLB
    L4_global     = 0x00000200, ///< pinned in the TLB
    XD            = 0,
    ATTRIBS_MASK  = 0x06,
  };
};

