#pragma once

#include <mem.h>
#include <cpu.h>
#include <fpu_state_ptr.h>

#include <globalconfig.h>

class Trap_state;

class Fpu_state
{
public:
  Unsigned32 fpcr, fpsr;
  Unsigned64 state[64]; // 32 128bit regs
};


struct Fpu_arch
{
  using Fpu_regs = Fpu_state;

  struct Exception_state_user
  {
  };

  static void save_user_exception_state(bool, Fpu_state_ptr const &, Trap_state *,
                                        Exception_state_user *)
  {}

  static void init(Cpu_number cpu, bool resume);

  static void save_state(Fpu_state *s);
  static void restore_state(Fpu_state const *s, bool owner);

  static unsigned state_size()
  { return sizeof (Fpu_regs); }

  static unsigned state_align()
  { return 16; }

#if ! defined (CONFIG_CPU_VIRT)
  static void init_state(Fpu_state *fpu_regs)
  {
    static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
  }

  static bool is_enabled()
  {
    Mword x;
    asm volatile ("mrs %0, CPACR_EL1" : "=r"(x));
    return x & (Cpu::Cpacr_el1_generic_hyp);
  }

  static void enable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "orr  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (Cpu::Cpacr_el1_generic_hyp));
    Mem::isb();
  }

  static void disable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "bic  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (Cpu::Cpacr_el1_generic_hyp));
    Mem::isb();
  }

#else // CONFIG_CPU_VIRT

  static void init_state(Fpu_state *fpu_regs)
  {
    static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
    //fpu_regs->fpexc |= FPEXC_EN;
  }

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

