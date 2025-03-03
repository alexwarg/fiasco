#pragma once

inline void
tramp_mp_setup_gic_info(void const *, unsigned)
{}

#include <globalconfig.h>

#ifdef CONFIG_MP
#include <paging-page.h>
#include <kmem.h>
#include <mem_unit.h>
#include <outer_cache.h>
#include <cpu.h>
#include <config.h>

inline Address
tramp_mp_prepare()
{
  extern char _tramp_mp_entry[];
  extern char _tramp_mp_startup_data_begin[];
  extern char _tramp_mp_startup_data_end[];
  extern volatile Mword _tramp_mp_startup_cp15_c1;
  extern volatile Mword _tramp_mp_startup_pdbr;
  extern volatile Mword _tramp_mp_startup_dcr;
  extern volatile Mword _tramp_mp_startup_ttbcr;
  extern volatile Mword _tramp_mp_startup_mair0;
  extern volatile Mword _tramp_mp_startup_mair1;

  _tramp_mp_startup_cp15_c1 = Cpu::sctlr;
  _tramp_mp_startup_pdbr
    = Kmem::kdir->virt_to_phys((Address)Kmem::kdir) | Page::Ttbr_bits;
  _tramp_mp_startup_ttbcr   = Page::Ttbcr_bits;
  _tramp_mp_startup_mair0   = Page::Mair0_prrr_bits;
  _tramp_mp_startup_mair1   = Page::Mair1_nmrr_bits;
  _tramp_mp_startup_dcr     = 0x55555555;

  __asm__ __volatile__ ("" : : : "memory");
  Mem_unit::clean_dcache(_tramp_mp_startup_data_begin, _tramp_mp_startup_data_end);

  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_cp15_c1));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_pdbr));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_dcr));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_ttbcr));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_mair0));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tramp_mp_startup_mair1));

  return Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry);
}

#endif
