#pragma once

#include <types.h>
#include <pre_parts.h>
#include <globalconfig.h>
#include <idt_init.h>

namespace Config
{
#if defined (CONFIG_BIT32)
 enum {
    PAGE_SHIFT          = ARCH_PAGE_SHIFT,
    PAGE_SIZE           = 1 << PAGE_SHIFT,
    PAGE_MASK           = ~( PAGE_SIZE - 1),

    SUPERPAGE_SHIFT     = 22,
    SUPERPAGE_SIZE      = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK      = ~( SUPERPAGE_SIZE - 1 ),

    Irq_shortcut        = 1,
  };
#endif
#if defined (CONFIG_BIT64)
  enum {
    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,
    PAGE_MASK  = ~( PAGE_SIZE - 1),

    SUPERPAGE_SHIFT = 21,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~( SUPERPAGE_SIZE -1 ),

    PDP_SIZE		= 1LL << 30,
    PML4_SIZE		= 1LL << 39,

    PML4E_SHIFT		= 39,
    PML4E_MASK		= 0x1ff,
    PDPE_SHIFT		= 30,
    PDPE_MASK		= 0x1ff,
    PDE_SHIFT		= 21,
    PDE_MASK		= 0x1ff,
    PTE_SHIFT		= 12,
    PTE_MASK		= 0x1ff,

    Irq_shortcut = 1,
  };
#endif

  enum
  {
#ifdef CONFIG_KERNEL_ISOLATION
    Access_user_mem = No_access_user_mem,
#else
    // can access user memory directly
    Access_user_mem = Access_user_mem_direct,
#endif
#ifdef CONFIG_IA32_PCID
    Pcid_enabled = true,
#else
    Pcid_enabled = false,
#endif

    /// Timer vector used with APIC timer or IOAPIC
    Apic_timer_vector = APIC_IRQ_BASE + 0,
  };

  extern unsigned scheduler_irq_vector;

  enum Scheduler_config
  {
    SCHED_PIT = 0,
    SCHED_RTC,
    SCHED_APIC,
    SCHED_HPET,

#ifdef CONFIG_SCHED_PIT
    Scheduler_mode        = SCHED_PIT,
    Scheduler_granularity = 1000U,
    Default_time_slice    = 10 * Scheduler_granularity,
#endif

#ifdef CONFIG_ONE_SHOT
    Scheduler_one_shot = true,
#else
    Scheduler_one_shot = false,
#endif

#ifdef CONFIG_SCHED_RTC
    Scheduler_mode = SCHED_RTC,
#  ifdef CONFIG_SLOW_RTC
    Scheduler_granularity = 15625U,
#  else
    Scheduler_granularity = 976U,
#  endif
    Default_time_slice = 10 * Scheduler_granularity,
#endif

#ifdef CONFIG_SCHED_APIC
    Scheduler_mode = SCHED_APIC,
#  ifdef CONFIG_ONE_SHOT
    Scheduler_granularity = 1U,
    Default_time_slice = 10000 * Scheduler_granularity,
#  else
    Scheduler_granularity = 1000U,
    Default_time_slice = 10 * Scheduler_granularity,
#  endif
#endif

#ifdef CONFIG_SCHED_HPET
    Scheduler_mode = SCHED_HPET,
    Scheduler_granularity = 1000U,
    Default_time_slice = 10 * Scheduler_granularity,
#endif
  };

  enum
  {
    Pic_prio_modify = true,
#ifdef CONFIG_SYNC_TSC
    Kip_clock_uses_rdtsc = true,
#else
    Kip_clock_uses_rdtsc = false,
#endif
  };

  extern bool apic;

#ifdef CONFIG_WATCHDOG
  extern bool watchdog;
#else
  constexpr bool watchdog = false;
#endif

  //  static const bool hlt_works_ok = false;
  extern bool hlt_works_ok;

  // the default uart to use for serial console
  constexpr unsigned default_console_uart = 1;
  constexpr unsigned default_console_uart_baudrate = 115200;

  extern bool found_vmware;
};

