#pragma once

#include <cpu_mask.h>
#include <globalconfig.h>

#ifdef CONFIG_MP

#include "cpu_call.h"

template<typename T>
class Mem_space_tlb
{
  T *self() { return static_cast<T *>(this); }
  T const *self() const { return static_cast<T *>(this); }

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

  void tlb_flush_all_cpus()
  {
    if (!T::Need_xcpu_tlb_flush)
      {
        self()->tlb_flush_current_cpu();
        return;
      }

    // To prevent a race condition that could potentially lead to the use of
    // outdated page table entries on other cores, we have to execute a memory
    // barrier that ensures that our PTE changes are visible to all other cores,
    // before we access tlb_active_on_cpu(). Otherwise, if the Mem_space gets
    // active on another core, shortly after we read tlb_active_on_cpu() where it
    // was reported as non-active, we won't send a TLB flush to the other core,
    // but it might not yet see our PTE changes.
    //self()->sync_read_tlb_active_on_cpu();

    auto *se = self();
    Cpu_call::cpu_call_many(active_tlb(), [se](Cpu_number)
      {
        se->tlb_flush_current_cpu();
        return false;
      });
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
  T *self() { return static_cast<T *>(this); }
  T const *self() const { return static_cast<T *>(this); }

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

  void tlb_flush_all_cpus()
  {
    self()->tlb_flush_current_cpu();
  }

};

#endif
