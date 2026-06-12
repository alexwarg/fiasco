#pragma once

#include <kmem.h>
#include <cassert>

void *
Kmem_mmio::map(Address phys, [[maybe_unused]] size_t size, bool, bool, bool)
{
  assert((phys + size <= Mem_layout::KSEG1e - Mem_layout::KSEG1)
         && "MMIO outside KSEG1");
  return reinterpret_cast<void *>(phys + Mem_layout::KSEG1);
}
