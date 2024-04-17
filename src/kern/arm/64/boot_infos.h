#pragma once

#include <globalconfig.h>
#include <types.h>

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))

#ifdef CONFIG_CPU_VIRT

struct Boot_paging_info
{
  void *l0_dir;
  void *scratch;
  Mword free_map;
};

#else

struct Boot_paging_info
{
  void *l0_dir;
  void *l0_vdir;
  void *scratch;
  Mword free_map;
};

#endif
