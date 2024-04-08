#pragma once

#include "paging.h"

namespace Kernel_task_kdir
{
  inline Pdir *get()
  {
    extern char kernel_l0_dir[] asm("kernel_l0_dir");
    return reinterpret_cast<Pdir*>(&kernel_l0_dir);
  }
};

