#pragma once

#include <per_cpu_data.h>
#include <types.h>

// maps APIC IDs to logical IDs
class Cpu_id_map
{
public:
  bool valid() const
  {
    return _valid;
  }

  Cpu_number find(Unsigned32 apic_id) const
  {
    for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
      if (_map[n] == apic_id)
        return n;

    return Cpu_number::nil();
  }

  void set(Cpu_number n, Unsigned32 apic_id)
  {
    _valid = true;
    _map[n] = apic_id;
  }

private:
  bool _valid = false;
  Per_cpu_array<Unsigned32> _map;
};

extern Cpu_id_map kernel_cpu_id_map;
