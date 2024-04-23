
#include <kernel_thread.h>
#include <mem_layout.h>

#include <globalconfig.h>

#ifdef CONFIG_MP

#include <platform_control.h>

void
Kernel_thread::boot_app_cpus()
{
  if (Config::Max_num_cpus <= 1)
    return;

  Platform_control::boot_all_secondary_cpus();
}
#endif

#if 0
IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  memset(const_cast<char *>(Mem_layout::initcall_start), 0,
         Mem_layout::initcall_end - Mem_layout::initcall_start);
  printf("%d KB kernel memory freed @ %p\n",
         (int)(Mem_layout::initcall_end - Mem_layout::initcall_start)/1024,
         Mem_layout::initcall_start);
}
#endif

FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  boot_app_cpus();
}
