
#include <kmem_space.h>
#include <globalconfig.h>
#include <boot_infos.h>
#include <paging.h>

// always 16kB also for LPAE we use 4 consecutive second level tables
static char kernel_page_directory[0x4000]
  __attribute__((aligned(0x4000), section(".bss.kernel_page_dir")));

// initialize the kernel space (page table)
void Kmem_space::init()
{
  unsigned domains = 0x0001;

  asm volatile("mcr p15, 0, %0, c3, c0" : : "r" (domains));

  Mem_unit::clean_vdcache();
}

#if defined (CONFIG_ARM_LPAE)


Unsigned64 kernel_lpae_dir[4] __attribute__((aligned(4 * sizeof(Unsigned64))));
Kpdir *Mem_layout::kdir = (Kpdir *)&kernel_lpae_dir;

static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory,
  kernel_lpae_dir
};

#else // CONFIG_ARM_LPAE

Kpdir *Mem_layout::kdir = (Kpdir *)&kernel_page_directory;

static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory
};

#endif // CONFIG_ARM_LPAE
