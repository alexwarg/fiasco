#pragma once

#include <construction.h>

#include "types.h"


#define DEFINE_PER_CPU_CTOR_DATA(id)

class Per_cpu_data_up
{
public:
  static void init_ctors() {}
  static void run_ctors(Cpu_number)
  {
    extern ctor_function_t __PER_CPU_INIT_ARRAY_START__[];
    extern ctor_function_t __PER_CPU_INIT_ARRAY_END__[];
    run_ctor_functions(__PER_CPU_INIT_ARRAY_START__, __PER_CPU_INIT_ARRAY_END__);

    extern ctor_function_t __PER_CPU_CTORS_LIST__[];
    extern ctor_function_t __PER_CPU_CTORS_END__[];
    run_ctor_functions(__PER_CPU_CTORS_LIST__, __PER_CPU_CTORS_END__);
  }

  static void run_late_ctors(Cpu_number)
  {
    extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_START__[];
    extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_END__[];
    run_ctor_functions(__PER_CPU_LATE_INIT_ARRAY_START__,
                       __PER_CPU_LATE_INIT_ARRAY_END__);

    extern ctor_function_t __PER_CPU_LATE_CTORS_LIST__[];
    extern ctor_function_t __PER_CPU_LATE_CTORS_END__[];
    run_ctor_functions(__PER_CPU_LATE_CTORS_LIST__, __PER_CPU_LATE_CTORS_END__);
  }

  static bool valid(Cpu_number cpu) noexcept
  {
#if defined NDEBUG
    (void)cpu;
    return 1;
#else
    return cpu == Cpu_number::boot_cpu();
#endif
  }

  enum With_cpu_num { Cpu_num };

protected:
  template<typename T>
  static T per_cpu_ref(T adr, Cpu_number)
  {
    return adr;
  }

  template<typename T>
  static void add_ctor_wo_arg(T *) noexcept
  {}

  template<typename T>
  static void add_ctor_w_arg(T *) noexcept
  {}
};

using Per_cpu_data = Per_cpu_data_up;


