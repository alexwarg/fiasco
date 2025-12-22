
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

extern "C" void bootstrap_main(unsigned long load_addr);
void bootstrap_main(unsigned long load_addr)
{
  if (load_addr)
    {
      Bootstrap::relocate(load_addr);

      // prevent compiler from reordering loads before applying the relocations
      Mem::barrier();

      bs_info.kernel_start_phys += load_addr;
      bs_info.kernel_end_phys   += load_addr;
    }

  bs_info.kernel_load_addr += load_addr;

  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging())
                    | Page::Ttbr_bits;

  // Attention zone:
  // Only touch loader's own memory here until paging enabled.

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  // force to construct an absolute relocation because GCC may not do it.
  bs_info.entry();

  L4::infinite_loop();
}

