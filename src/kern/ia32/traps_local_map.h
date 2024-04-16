
#pragma once

#include <kmem.h>
#include <context.h>
#include <globalconfig.h>

#ifdef CONFIG_CPU_LOCAL_MAP

inline bool
update_local_map(Context *c, Address pfa, Mword /*error_code*/)
{
  // This function assumes 4-level paging on AMD64. The page map level 4 table
  // is indexed by bits 47..39 of a linear address. Thus each entry covers 512G.
  static_assert(255 == (Mem_layout::User_max >> 39),
                "Mem_layout::User_max must lie in 512G slot 255.");
  // 512G slot 259 is used for context-specific kernel data.
  static_assert(259 == ((Mem_layout::Io_bitmap >> 39) & 0x1ff),
                "Mem_layout::Io_bitmap must lie in 512G slot 259.");
  static_assert(259 == ((Mem_layout::Caps_start >> 39) & 0x1ff),
                "Mem_layout::Caps_start must lie in 512G slot 259.");
  static_assert(259 == (((Mem_layout::Caps_end - 1) >> 39) & 0x1ff),
                "Mem_layout::Caps_end - 1 must lie in 512G slot 259.");

  unsigned idx = (pfa >> 39) & 0x1ff;
  if (EXPECT_FALSE((idx > 255) && idx != 259))
    return false;

  auto *m = Kmem::pte_map();
  if (EXPECT_FALSE(m->operator [](idx)))
    return false;

  auto s = Kmem::current_cpu_udir()->walk(Virt_addr(pfa), 0);
  assert (!s.is_valid());
  auto r = c->vcpu_aware_space()->dir()->walk(Virt_addr(pfa), 0);
  if (EXPECT_FALSE(!r.is_valid()))
    return false;

  m->set_bit(idx);
  *s.pte = *r.pte;
  return true;
}


#else // CONFIG_CPU_LOCAL_MAP

inline bool
update_local_map(Context *, Address, Mword)
{ return false; }

#endif // CONFIG_CPU_LOCAL_MAP
