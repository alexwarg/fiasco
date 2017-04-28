#pragma once

#include <types.h>
#include <context_cpu_state_arm.h>
#include <globalconfig.h>

template<typename BASE>
class Context_cpu_state_arch : public BASE, public Context_cpu_state_arm
{
public:
  explicit Context_cpu_state_arch(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  void prepare_switch_to(void (*fptr)())
  {
    this->kernel_sp -= 2;
    *reinterpret_cast<void (**)()> (this->kernel_sp) = fptr;
  }

#ifdef CONFIG_ARM_V6PLUS
  void store_tpidrurw()
  {
    asm volatile ("mrc p15, 0, %0, c13, c0, 2" : "=r" (_tpidrurw));
  }

  void load_tpidrurw() const
  {
    asm volatile ("mcr p15, 0, %0, c13, c0, 2" : : "r" (_tpidrurw));
  }

  void store_tpidruro()
  {
    asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(_tpidruro));
  }

  void load_tpidruro() const
  {
    asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r" (_tpidruro));
  }
#endif
};
