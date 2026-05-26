#pragma once

#include <globalconfig.h>

class Jdb_monitored_mem
{
public:
  template< typename T >
  static void set_monitored_address(T *dest, T val)
  {
    *const_cast<T volatile *>(dest) = val;
#ifdef CONFIG_MP
    Mem::mp_wmb();
#endif
  }

  template< typename T >
  static T monitor_address(Cpu_number, T const volatile *addr)
  {
    return *addr;
  }
};

