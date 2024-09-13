#pragma once

#include <globalconfig.h>
#include <types.h>

#ifdef CONFIG_MP

#include <apic.h>

inline Cpu_number
dbg_find_cpu()
{
  Apic_id phys_cpu = Apic::get_id();
  Cpu_number log_cpu = Apic::find_cpu(phys_cpu);
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n", cxx::int_value<Apic_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }
  return log_cpu;
}

#else // CONFIG_MP

inline Cpu_number
dbg_find_cpu() { return Cpu_number::boot_cpu(); }

#endif // CONFIG_MP

