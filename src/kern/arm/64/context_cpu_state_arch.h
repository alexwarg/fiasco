#pragma once

#include <types.h>
#include <context_cpu_state_arm.h>
#include <alternatives.h>

template<typename BASE>
class Context_cpu_state_arch : public BASE, public Context_cpu_state_arm
{
  Mword _tpidr2rw;
public:
  explicit Context_cpu_state_arch(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  void prepare_switch_to(void (*fptr)())
  {
    this->kernel_sp -= 2;
    *reinterpret_cast<void (**)()> (this->kernel_sp) = fptr;
  }

  void store_tpidrurw()
  {
    asm volatile ("mrs %0, TPIDR_EL0" : "=r" (_tpidrurw));
    asm volatile (ALTERNATIVE_INSN("nop", "mrs %0, S3_3_c13_c0_5") // TPIDR2_EL0, bintutils >= 2.38
                  : "=r" (_tpidr2rw) : [alt_probe] "i"(Cpu::has_sme::probe));
  }

  void load_tpidrurw() const
  {
    asm volatile ("msr TPIDR_EL0, %0" : : "r" (_tpidrurw));
    asm volatile (ALTERNATIVE_INSN("nop", "msr S3_3_c13_c0_5, %0") // TPIDR2_EL0, bintutils >= 2.38
                  : : "r" (_tpidr2rw), [alt_probe] "i"(Cpu::has_sme::probe));
  }

  void store_tpidruro()
  {
    asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(_tpidruro));
  }

  void load_tpidruro() const
  {
    asm volatile ("msr TPIDRRO_EL0, %0" : : "r" (_tpidruro));
  }
};
