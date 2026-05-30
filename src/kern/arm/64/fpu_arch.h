#pragma once

#include <mem.h>
#include <cpu.h>
#include <fpu_state_ptr.h>

#include <globalconfig.h>
#ifdef CONFIG_ARM_SVE
#include <fpu_arm_sve.h>
#else
#include <fpu_state_arm_simd.h>

class Fpu_state : public Fpu_state_simd {};

struct Fpu_arch_base
{
  static void inline init(Cpu_number, bool) {}

  static unsigned state_size()
  { return sizeof (Fpu_state); }
};

#endif

class Trap_state;

struct Fpu_arch : Fpu_arch_base
{
  struct Exception_state_user
  {
  };

  static void save_user_exception_state(bool, Fpu_state_ptr const &, Trap_state *,
                                        Exception_state_user *)
  {}

  static void init(Cpu_number cpu, bool resume);

  static void copy_state(Fpu_state *to, Fpu_state const *from)
  {
    to->copy(from);
  }

  static void save_state(Fpu_state *s) { s->save(); }
  static void restore_state(Fpu_state const *s, bool owner)
  { s->restore(); (void)owner; }

  constexpr static unsigned state_align()
  { return 16; }

#if ! defined (CONFIG_CPU_VIRT)
  static bool is_enabled()
  {
    Mword x;
    asm volatile ("mrs %0, CPACR_EL1" : "=r"(x));
    return x & (Cpu::Cpacr_el1_fpen_full);
  }

  static void enable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "orr  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (Cpu::Cpacr_el1_fpen_full));
    Mem::isb();
  }

  static void disable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "bic  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (Cpu::Cpacr_el1_fpen_full));
    Mem::isb();
  }

#else // CONFIG_CPU_VIRT

  static bool is_enabled()
  {
    Mword dummy;
    __asm__ __volatile__ ("mrs %0, CPTR_EL2" : "=r"(dummy));
    return !(dummy & (Cpu::Cptr_el2_tfp));
  }


  static void enable()
  {
    Mword dummy;
    __asm__ __volatile__ (
        "mrs %0, CPTR_EL2 \n"
        "bic %0, %0, %1   \n"
        "msr CPTR_EL2, %0 \n"
        : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tfp));
    Mem::isb();
  }

  static void disable()
  {
    Mword dummy;
    __asm__ __volatile__ (
        "mrs  %0, CPTR_EL2 \n"
        "orr  %0, %0, %1   \n"
        "msr  CPTR_EL2, %0 \n"
        : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tfp));
    Mem::isb();
  }
#endif // CONFIG_CPU_VIRT
};

