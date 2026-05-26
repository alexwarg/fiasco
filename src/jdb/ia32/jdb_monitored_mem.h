#pragma once

#include <cpu.h>

class Jdb_monitored_mem
{
public:
  template< typename T >
  static void set_monitored_address(T *dest, T val)
  { *const_cast<T volatile *>(dest) = val; }

  template< typename T >
  static T monitor_address(Cpu_number current_cpu, T const volatile *addr)
  {
    if (!*addr && Cpu::cpus.cpu(current_cpu).has_monitor_mwait())
      {
        asm volatile ("monitor \n" : : "a"(addr), "c"(0), "d"(0) );
        Mword irq_sup = Cpu::cpus.cpu(current_cpu).has_monitor_mwait_irq() ? 1 : 0;
        asm volatile ("mwait   \n" : : "a"(0x00), "c"(irq_sup) );
      }

    return *addr;
  }
};


