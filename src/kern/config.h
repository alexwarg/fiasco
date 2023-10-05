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


#ifdef CONFIG_FINE_GRAINED_CPUTIME
    Fine_grained_cputime = true,
#else
    Fine_grained_cputime = false,
#endif

#ifdef CONFIG_STACK_DEPTH
    Stack_depth = true,
#else
    Stack_depth = false,
#endif
#ifdef CONFIG_NO_FRAME_PTR
    Have_frame_ptr = 0,
#else
    Have_frame_ptr = 1,
#endif
#ifdef CONFIG_JDB
    Jdb = 1,
#else
    Jdb = 0,
#endif
#ifdef CONFIG_JDB_LOGGING
    Jdb_logging = 1,
#else
    Jdb_logging = 0,
#endif
#ifdef CONFIG_JDB_ACCOUNTING
    Jdb_accounting = 1,
#else
    Jdb_accounting = 0,
#endif
#ifdef CONFIG_MP
    Max_num_cpus = CONFIG_MP_MAX_CPUS,
#else
    Max_num_cpus = 1,
#endif
#ifdef CONFIG_BIG_ENDIAN
    Big_endian = true,
#else
    Big_endian = false,
#endif
  };

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
#endif
#if defined(CONFIG_BIT64)
  // 6 percent of total RAM, >=55466MB RAM => 3328MB kmem
  constexpr unsigned kmem_per_cent() { return 6; }
  constexpr unsigned long kmem_max() { return 3328UL << 20; }
#endif

  unsigned long kmem_size(unsigned long available_size);

#if defined (CONFIG_SERIAL)
  extern int serial_esc;
#else
  constexpr int serial_esc = 0;
#endif
}

#define GREETING_COLOR_ANSI_TITLE  "\033[1;32m"
#define GREETING_COLOR_ANSI_INFO   "\033[0;32m"

#if defined (CONFIG_IA32)
#define TARGET_NAME "x86-32"
#endif

#if defined (CONFIG_AMD64)
#define TARGET_NAME "x86-64"
#endif

#define CONFIG_KERNEL_VERSION_STRING \
  GREETING_COLOR_ANSI_TITLE "Welcome to L4/Fiasco.OC!\\n"                      \
  GREETING_COLOR_ANSI_INFO "L4/Fiasco.OC microkernel on " CONFIG_XARCH "\\n"      \
                           "Rev: " CODE_VERSION " compiled with " COMPILER \
                           TARGET_NAME_PHRASE "    [" CONFIG_LABEL "]\\n"    \
                           "Build: #" BUILD_NR " " BUILD_DATE "\\n"            \
  GREETING_COLOR_ANSI_OFF


#if ! defined (CONFIG_VIRTUAL_SPACE_IFACE)
#define FIASCO_SPACE_VIRTUAL
#else
#define FIASCO_SPACE_VIRTUAL virtual
#endif

#if ! defined(CONFIG_VIRTUAL_SPACE_IFACE)
#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE
#else
#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE override
#endif

