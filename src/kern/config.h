#pragma once

#include <config_arch.h>
#include <globalconfig.h>
#include <config_tcbsize.h>
#include <l4_types.h>

// special magic to allow old compilers to inline constants

#if defined(__clang__)
# define COMPILER "clang " __clang_version__
# define GCC_VERSION 409
#else
#if defined(__GNUC__)
# define COMPILER "gcc " __VERSION__
# define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
# define COMPILER "Non-GCC"
# define GCC_VERSION 0
#endif
#endif

#define GREETING_COLOR_ANSI_OFF    "\033[0m"

#define FIASCO_KERNEL_SUBVERSION 3

namespace Config
{
  extern const char *const kernel_warn_config_string;
  enum {
    SERIAL_ESC_IRQ	= 2,
    SERIAL_ESC_NOIRQ	= 1,
    SERIAL_NO_ESC	= 0,
  };

  void init();
  void init_arch();

  // global kernel configuration
  enum
  {
    Kernel_version_id = 0x87004444 | (FIASCO_KERNEL_SUBVERSION << 16), // "DD....."
    // kernel (idle) thread prio
    Kernel_prio = 0,
    // default prio
    Default_prio = 1,

    Warn_level = CONFIG_WARN_LEVEL,

    One_shot_min_interval_us =   200,
    One_shot_max_interval_us = 10000,
    Rcu_grace_period = 1000,

    Fine_grained_cputime = IS_ENABLED(CONFIG_FINE_GRAINED_CPUTIME),

    Stack_depth    = IS_ENABLED(CONFIG_STACK_DEPTH),
    Have_frame_ptr = ! IS_ENABLED(CONFIG_NO_FRAME_PTR),
    Jdb            = IS_ENABLED(CONFIG_JDB),
    Jdb_logging    = IS_ENABLED(CONFIG_JDB_LOGGING),
    Jdb_accounting = IS_ENABLED(CONFIG_JDB_ACCOUNTING),
#ifdef CONFIG_MP
    Max_num_cpus = CONFIG_MP_MAX_CPUS,
#else
    Max_num_cpus = 1,
#endif
    Big_endian = IS_ENABLED(CONFIG_BIG_ENDIAN),
  };

  static constexpr bool Scheduler_one_shot = IS_ENABLED(CONFIG_ONE_SHOT);

  constexpr Cpu_number max_num_cpus() { return Cpu_number(Max_num_cpus); }

  extern bool getchar_does_hlt_works_ok;
  extern bool esc_hack;
  extern unsigned tbuf_entries;

  constexpr Order page_order()
  { return Order(PAGE_SHIFT); }

  constexpr Bytes page_size()
  { return Bytes(PAGE_SIZE); }

#if defined (CONFIG_BIT32)
  // 8 percent of total RAM, >=750MB RAM => 60MB kmem
  constexpr unsigned kmem_per_cent() { return 8; };
  constexpr unsigned long kmem_max() { return 60UL << 20; }
# define TARGET_WORD_LEN "32"
#endif
#if defined(CONFIG_BIT64)
  // 6 percent of total RAM, >=55466MB RAM => 3328MB kmem
  constexpr unsigned kmem_per_cent() { return 6; }
  constexpr unsigned long kmem_max() { return 3328UL << 20; }
# define TARGET_WORD_LEN "64"
#endif

  unsigned long kmem_size(unsigned long available_size);

  // Attention: this enum is used by the Lauterbach Trace32 OS awareness.
  enum Ext_vcpu_info
  {
    Ext_vcpu_infos_offset = 0x200,
    Ext_vcpu_state_offset = 0x400,
  };

  static constexpr Mword ext_vcpu_size()
  { return PAGE_SIZE; }

#if defined (CONFIG_SERIAL)
  extern int serial_esc;
#else
  constexpr int serial_esc = 0;
#endif
}

#define GREETING_COLOR_ANSI_TITLE  "\033[1;32m"
#define GREETING_COLOR_ANSI_INFO   "\033[0;32m"

#if defined (CONFIG_IA32) || defined (CONFIG_AMD64)
#define DISPLAY_ARCH "x86"
#else
#define DISPLAY_ARCH CONFIG_XARCH
#endif


#define CONFIG_KERNEL_VERSION_STRING \
  GREETING_COLOR_ANSI_TITLE "Welcome to the L4Re Microkernel!\\n"         \
  GREETING_COLOR_ANSI_INFO "L4Re Microkernel on " DISPLAY_ARCH "-" TARGET_WORD_LEN "\\n"      \
                           "Rev: " CODE_VERSION " compiled with " COMPILER \
                           TARGET_NAME_PHRASE "    [" CONFIG_LABEL "]\\n"    \
                           "Build: #" BUILD_NR " " BUILD_DATE "\\n"            \
  GREETING_COLOR_ANSI_OFF


#if ! defined (CONFIG_VIRTUAL_SPACE_IFACE)
#define FIASCO_SPACE_VIRTUAL
#define FIASCO_SPACE_OVERRIDE
#else
#define FIASCO_SPACE_VIRTUAL virtual
#define FIASCO_SPACE_OVERRIDE override
#endif

