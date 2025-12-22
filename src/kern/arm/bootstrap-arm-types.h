#pragma once

#include <types.h>
#include <cxx/cxx_int>
#include <boot_infos.h>
#include <paging-page.h>
#include <bootstrap.h>

#include <globalconfig.h>

struct Bootstrap_info
{
  void (*entry)();
  void *kip;
  Address kernel_start_phys;
  Address kernel_end_phys;
  Address kernel_load_addr;
  Boot_paging_info pi;
};

extern Bootstrap_info bs_info;

namespace Bootstrap
{
  struct Order_t;
  struct Virt_addr_t;
  struct Phys_addr_t;

  using Order = cxx::int_type<unsigned, Order_t>;
  using Virt_addr = cxx::int_type_order<Mword, Virt_addr_t, Order>;
#ifdef CONFIG_ARM_LPAE
  using Phys_addr = cxx::int_type_order<Unsigned64, Phys_addr_t, Order>;
#else
  using Phys_addr = cxx::int_type_order<Unsigned32, Phys_addr_t, Order>;
#endif
#ifdef CONFIG_ARM_LPAE
  static constexpr Order map_page_order{21};
#else
  static constexpr Order map_page_order{20};
#endif
  static constexpr Phys_addr map_page_size_phys = Phys_addr(1) << map_page_order;
  static constexpr Virt_addr map_page_size      = Virt_addr(1) << map_page_order;

#ifdef CONFIG_ARM_LPAE
  constexpr
  Phys_addr pt_entry(Phys_addr pa, bool cache, bool local)
  {
    return cxx::mask_lsb(pa, map_page_order) | Phys_addr(1) // this is a block
      | Phys_addr(1 << 10) // AF
      | Phys_addr(3 << 8)  // Inner sharable
      | Phys_addr(local ? 1 << 11 : 0) // nG flag
      | Phys_addr(cache ? 8 : 1ULL << 54) // assume XN for non-cachable memory
      ;
  }
#else // CONFIG_ARM_LPAE
  constexpr
  Phys_addr pt_entry(Phys_addr pa, bool cache, bool local)
  {
    return cxx::mask_lsb(pa, map_page_order)
                  | Phys_addr(cache ? Page::Section_cachable : Page::Section_no_cache)
                  | Phys_addr(local ? Page::Section_local : Page::Section_global);
  }
#endif // ! CONFIG_ARM_LPAE

  inline
  void map_memory(void volatile *pd, Virt_addr va, Phys_addr pa,
                  bool cache, bool local)
  {
    Phys_addr *const p = static_cast<Phys_addr *>(const_cast<void *>(pd));
    p[cxx::int_value<Virt_addr>(va >> map_page_order)]
      = pt_entry(pa, cache, local);
  }

  enum
  {
    Cache_flush_area = 0 // needed for XScale and StrongARM (just ignore them)

    // SA1100: Cache_flush_area = 0xe0000000
    // XSCALE: Cache_flush_area = 0xa0100000
  };

  [[gnu::pure]] inline Address virt_ofs()
  {
    return bs_info.kernel_load_addr - Mem_layout::Map_base;
  }

  inline ALWAYS_INLINE
  void *kern_to_boot(void *a)
  {
    return offset_cast<void *>(a, virt_ofs());
  }
}

