#pragma once

#include <paging-page.h>
#include <kmem.h>
#include <mem_unit.h>
#include <cpu.h>
#include <config.h>
#include <processor.h>
#include <kernel_task.h>

#include <pre_parts.h>
#include <globalconfig.h>

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

#if defined (CONFIG_HAVE_ARM_GICV2) && defined (PRE_pic_gic)

inline void boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = Mem_layout::Gic_dist_phys_base;
  inf->gic_cpu_base = Mem_layout::Gic_cpu_phys_base;
}

#else // GIC

inline void boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = 0;
}

#endif // GIC

inline Address
tramp_mp_prepare()
{
  extern char _tramp_mp_entry[];
  extern char _tramp_mp_boot_info[];
  Mp_boot_info volatile *_tmp;
  _tmp = reinterpret_cast<Mp_boot_info*>(_tramp_mp_boot_info);

  _tmp->sctlr = Proc::sctlr();
  _tmp->mair  = Page::Mair0_prrr_bits;
  _tmp->ttbr_kern = Kmem::kdir->virt_to_phys((Address)Kmem::kdir);
  if (!Proc::Is_hyp)
    _tmp->ttbr_usr = cxx::int_value<Phys_mem_addr>(Kernel_task::kernel_task()->dir_phys());

  _tmp->tcr   = Page::Ttbcr_bits;
  boot_app_cpu_gic(_tmp);

  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache();

  return Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry);
}

