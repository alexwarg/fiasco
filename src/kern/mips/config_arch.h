#pragma once

#include <types.h>
#include <pre_parts.h>
#include <globalconfig.h>

namespace Config
{
  enum
  {
    //Access_user_mem = Access_user_mem_direct,
    Access_user_mem = No_access_user_mem,
    //Access_user_mem = Must_access_user_mem_direct,

    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,
    PAGE_MASK  = ~(PAGE_SIZE - 1),
    SUPERPAGE_SHIFT = 21, // MUST be a odd number!
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~(SUPERPAGE_SIZE -1),

    // XXXKYMA: No large pages/TLBs yet, update paging-mips32 when enabled
    have_superpages = 0,
    hlt_works_ok = 1,
    Irq_shortcut = 1,
  };

  enum
  {
    Kmem_per_cent = 6,
    Kmem_max_mb   = 32,
  };

  enum
  {
    Scheduler_one_shot		= 0,
    Scheduler_granularity	= 1000UL,
    Default_time_slice	        = 10 * Scheduler_granularity,
    Cpu_frequency               = CONFIG_MIPS_CPU_FREQUENCY,
  };

  enum
  {
    // Defaults for Kernel_uart ...
    default_console_uart = 0,
    default_console_uart_baudrate = 115200
  };
}

