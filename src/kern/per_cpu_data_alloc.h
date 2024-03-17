#pragma once

#include "per_cpu_data.h"

class Per_cpu_data_alloc : public Per_cpu_data
{
public:
  static bool alloc(Cpu_number cpu);
};


#if !defined (CONFIG_MP)
inline
bool Per_cpu_data_alloc::alloc(Cpu_number)
{ return true; }
#endif


