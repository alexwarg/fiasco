#pragma once

#include "kmem.h"

namespace Kernel_task_kdir
{
  inline Pdir *get() { return reinterpret_cast<Pdir *>(Kmem::kdir); }
};
