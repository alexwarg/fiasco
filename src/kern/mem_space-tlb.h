#pragma once

#include <cpu_mask.h>
#include <globalconfig.h>

#ifdef CONFIG_MP

template<typename T>
class Mem_space_tlb
{
public:
  static constexpr bool Need_xcpu_tlb_flush = true;

  static Cpu_mask const &active_tlb()
  {
    return _tlb_active;
  }

  static void enable_tlb(Cpu_number cpu)
  {
    _tlb_active.atomic_set(cpu);
  }

  static void disable_tlb(Cpu_number cpu)
  {
    _tlb_active.atomic_clear(cpu);
  }

private:
  static Cpu_mask _tlb_active;
};

template<typename T>
Cpu_mask Mem_space_tlb<T>::_tlb_active;

#else

template<typename T>
class Mem_space_tlb
{
public:
  static constexpr bool Need_xcpu_tlb_flush = false;

  static void enable_tlb(Cpu_number)
  {}

  static void disable_tlb(Cpu_number)
  {}

  static Cpu_mask active_tlb()
  {
    Cpu_mask c;
    c.set(Cpu_number::boot_cpu());
    return c;
  }
};

#endif
