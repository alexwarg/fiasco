
#pragma once

#include <types.h>

class Kmem_generic_api
{
public:
  static Address mmio_remap(Address phys, Address size, bool cache = false, bool with_exec = false);
};
