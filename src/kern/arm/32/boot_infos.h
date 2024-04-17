#pragma once

#include <globalconfig.h>

#define FIASCO_BOOT_PAGING_INFO \
  __attribute__((section(".bootstrap.info"), used))

#ifdef CONFIG_ARM_LPAE

struct Boot_paging_info
{
  void *kernel_page_directory;
  void *kernel_lpae_dir;
};

#else

struct Boot_paging_info
{
  void *kernel_page_directory;
};

#endif
