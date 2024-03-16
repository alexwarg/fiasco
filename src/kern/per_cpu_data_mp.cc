#include "per_cpu_data.h"

#include "panic.h"
#include <construction.h>
#include <cstring>

Per_cpu_data_mp::Offset_array Per_cpu_data_mp::_offsets;
unsigned Per_cpu_data_mp::late_ctor_start;
Per_cpu_data_mp::Ctor_vector Per_cpu_data_mp::ctors;

void
Per_cpu_data_mp::Ctor_vector::push_back(Ctor::Func func, void *base)
{
  extern Ctor _per_cpu_ctor_data_start[];
  extern Ctor _per_cpu_ctor_data_end[];

  if (_per_cpu_ctor_data_start + _len >= _per_cpu_ctor_data_end)
    panic("out of per_cpu_ctor_space");

  _per_cpu_ctor_data_start[_len++] = Ctor(func, base);
}

void
Per_cpu_data_mp::init_ctors()
{
  for (Offset_array::iterator i = _offsets.begin(); i != _offsets.end(); ++i)
    *i = -1;
}

void
Per_cpu_data_mp::run_ctors(Cpu_number cpu)
{
  extern ctor_function_t __PER_CPU_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_INIT_ARRAY_END__[];
  extern ctor_function_t __PER_CPU_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_CTORS_END__[];
  if (cpu == Cpu_number::boot_cpu())
    {
      run_ctor_functions(__PER_CPU_INIT_ARRAY_START__, __PER_CPU_INIT_ARRAY_END__);
      run_ctor_functions(__PER_CPU_CTORS_LIST__, __PER_CPU_CTORS_END__);
      late_ctor_start = ctors.len();
      return;
    }

  for (unsigned i = 0; i < late_ctor_start; ++i)
    ctors[i].exec(cpu);
}

void
Per_cpu_data_mp::run_late_ctors(Cpu_number cpu)
{
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_END__[];
  extern ctor_function_t __PER_CPU_LATE_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_LATE_CTORS_END__[];
  if (cpu == Cpu_number::boot_cpu())
    {
      run_ctor_functions(__PER_CPU_LATE_INIT_ARRAY_START__, __PER_CPU_LATE_INIT_ARRAY_END__);
      run_ctor_functions(__PER_CPU_LATE_CTORS_LIST__, __PER_CPU_LATE_CTORS_END__);
      return;
    }

  unsigned c = ctors.len();
  for (unsigned i = late_ctor_start; i < c; ++i)
    ctors[i].exec(cpu);
}

