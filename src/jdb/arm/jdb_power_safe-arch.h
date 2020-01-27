#pragma once

#include <ipi.h>
#include <processor.h>

namespace Jdb_power_safe {

inline void other_cpu_halt_in_jdb()
{
  Proc::halt();
}

inline void wakeup_other_cpus_from_jdb(Cpu_number c)
{
  Ipi::send(Ipi::Debug, Cpu_number::first(), c);
}

}
