#pragma once

#include <globalconfig.h>
#ifndef CONFIG_MP

inline void
tramp_mp_setup_gic_info(void const *, unsigned)
{}

#else

#include <paging-page.h>
#include <kmem.h>
#include <mem_unit.h>
#include <cpu.h>
#include <config.h>
#include <processor.h>
#include <kernel_task.h>
#include <pic-gic-helper.h>

struct Mp_boot_info
{
  Mword sctlr;
  Mword tcr;
  Mword mair;
  Mword ttbr_kern;
  Mword ttbr_usr;
  Mword gic_dist_base; // only needed for IGROUPR0
  Mword gic_cpu_base;
};

extern Mp_boot_info volatile _tramp_mp_boot_info;

inline void
tramp_mp_setup_gic_info(Pic_gic::Gic_info const *inf, unsigned version)
{
  if (!inf || !version)
    {
      _tramp_mp_boot_info.gic_dist_base = 0;
      return;
    }

  _tramp_mp_boot_info.gic_dist_base = inf->dist_phys;
  if (version == 2)
    _tramp_mp_boot_info.gic_cpu_base = inf->cpu_phys;
  else
    _tramp_mp_boot_info.gic_cpu_base = 0;
}

inline Address
tramp_mp_prepare()
{
  extern char _tramp_mp_entry[];
  Mp_boot_info volatile *_tmp = &_tramp_mp_boot_info;

  _tmp->sctlr = Proc::sctlr();
  _tmp->mair  = Page::Mair0_prrr_bits;
  _tmp->ttbr_kern = Kmem::kdir->virt_to_phys((Address)Kmem::kdir);
  if (!Proc::Is_hyp)
    _tmp->ttbr_usr = cxx::int_value<Phys_mem_addr>(Kernel_task::kernel_task()->dir_phys());

  _tmp->tcr   = Page::Ttbcr_bits;

  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache();

  return Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry);
}

#endif
