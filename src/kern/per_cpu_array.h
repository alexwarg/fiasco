#pragma once

#include "config.h"
#include "types.h"

template< typename T, unsigned EXTRA = 0 >
class Per_cpu_array
: public cxx::array<T, Cpu_number, Config::Max_num_cpus + EXTRA>
{};


