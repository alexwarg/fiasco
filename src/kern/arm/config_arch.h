#pragma once

#include <config_arm_bsp.h>
#include <types.h>
#include <globalconfig.h>

namespace Config
{

#if ! defined (CONFIG_ARM_V6PLUS)
  enum { Access_user_mem = Must_access_user_mem_direct };
#else
  enum { Access_user_mem = No_access_user_mem };
#endif

  enum
  {
    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,

    hlt_works_ok = 1,
    Irq_shortcut = 1,
  };

  enum
  {
#ifdef CONFIG_ONE_SHOT
    Scheduler_one_shot		= 1,
    Scheduler_granularity	= 1UL,
    Default_time_slice	        = 10000 * scheduler_granularity,
#else
    Scheduler_one_shot		= 0,
    Scheduler_granularity	= CONFIG_SCHED_GRANULARITY,
    Default_time_slice	        = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
#endif
  };

  enum : unsigned long
  {
    KMEM_SIZE = 16 << 20,
  };

  // the default uart to use for serial console
  static constexpr unsigned default_console_uart	= 3;
  static constexpr unsigned default_console_uart_baudrate = 115200;

  enum
  {
    Cache_enabled = true,
  };

  enum
  {
#ifdef CONFIG_ARM_ENABLE_SWP
    Cp15_c1_use_swp_enable = 1,
#else
    Cp15_c1_use_swp_enable = 0,
#endif
#ifdef CONFIG_ARM_ALIGNMENT_CHECK
    Cp15_c1_use_alignment_check = 1,
#else
    Cp15_c1_use_alignment_check = 0,
#endif

    Support_arm_linux_cache_API = 1,
  };

  enum
  {
#ifdef CONFIG_SYNC_CLOCK
    Kip_clock_uses_timer = 1,
#else
    Kip_clock_uses_timer = 0,
#endif
  };

#if defined (CONFIG_ARM_LPAE)
  enum
  {
    SUPERPAGE_SHIFT = 21,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
  };
#else
  enum
  {
    SUPERPAGE_SHIFT = 20,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
  };
#endif

  inline void init_arch() {}
};

