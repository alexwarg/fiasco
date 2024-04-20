
#include <config.h>
#include <cpu.h>
#include <fpu.h>
#include <ipi.h>
#include <kernel_task.h>
#include <kernel_uart.h>
#include <kip_init.h>
#include <kmem_alloc.h>
#include <kmem_space.h>
#include <per_cpu_data.h>
#include <per_cpu_data_alloc.h>
#include <pic.h>
#include <platform_control.h>
#include <psci.h>
#include <processor.h>
#include <static_init.h>
#include <thread.h>
#include <timer.h>
#include <utcb_init.h>
#include <arm_ipis.h>

#include <cstdlib>
#include <cstdio>

#include <globalconfig.h>

#if defined (CONFIG_BIT32) && ! defined (CONFIG_CPU_VIRT)

FIASCO_INIT
static void add_initial_pmem()
{
    // The first 4MB of phys memory are always mapped to Map_base
  Mem_layout::add_pmem(Mem_layout::Sdram_phys_base, Mem_layout::Map_base,
                       4 << 20);
}

STATIC_INITIALIZER_P(add_initial_pmem, 101);

#endif // CONFIG_BIT32 && !CONFIG_CPU_VIRT

FIASCO_INIT
static void stage1()
{
  Kernel_uart::init(Kernel_uart::Init_after_mmu);
  Proc::cli();
  Cpu::early_init();
  Config::init();
}

STATIC_INITIALIZER_P(stage1, STARTUP1_INIT_PRIO);

FIASCO_INIT
static void stage2()
{
  Cpu_number const boot_cpu = Cpu_number::boot_cpu();
  puts("Hello from Startup::stage2");
  Mem_space::init_page_sizes();

  Kip_init::init();
  Kmem_alloc::init();

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(boot_cpu);
  Per_cpu_data::run_ctors(boot_cpu);

  Kmem_space::init();
  Kernel_task::init();
  Mem_space::kernel_space(Kernel_task::kernel_task());
  Cpu::cpus.cpu(boot_cpu).init(false, true);
  Pic::init();
  Arm_ipis::init_per_cpu(boot_cpu, false);

  Platform_control::init(boot_cpu);
  Psci::init(boot_cpu);
  Fpu::init(boot_cpu, false);
  Ipi::init(boot_cpu);
  Timer::init(boot_cpu);
  Kip_init::init_kip_clock();
  Utcb_init::init();
}

STATIC_INITIALIZER_P(stage2, STARTUP_INIT_PRIO);

