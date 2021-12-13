
#include <bootstrap-arm-bits.h>
#include <infinite_loop.h>
#include <globalconfig.h>

#include <cstddef>

Bootstrap_info FIASCO_BOOT_PAGING_INFO bs_info;

#ifndef CONFIG_ARM_1176_CACHE_ALIAS_FIX
namespace Bootstrap {
  inline void do_arm_1176_cache_alias_workaround() {}
}
#endif


extern "C" void bootstrap_main();
void bootstrap_main()
{
  Bootstrap::load_addr = Bootstrap::relocate();

  bs_info.kernel_start_phys += Bootstrap::load_addr;
  bs_info.kernel_end_phys   += Bootstrap::load_addr;

  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging())
                    | Page::Ttbr_bits;

  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  // force to construct an absolute relocation because GCC may not do it.
  bs_info.entry();

  L4::infinite_loop();
}

