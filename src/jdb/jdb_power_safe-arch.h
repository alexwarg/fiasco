
#pragma once

#include <processor.h>

namespace Jdb_power_safe {

inline void other_cpu_halt_in_jdb()
{
  Proc::pause();
}

inline void wakeup_other_cpus_from_jdb(Cpu_number)
{}

}
