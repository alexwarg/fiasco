#pragma once

#include <mem.h>
#include <fpu_state.h>

#include <globalconfig.h>

class Trap_state;

struct Fpu_arch
{
  struct Exception_state_user
  {
  };

  struct Fpu_regs
  {
    Unsigned32 fpcr, fpsr;
    Unsigned64 state[64]; // 32 128bit regs
  };

  static void save_user_exception_state(bool, Fpu_state *, Trap_state *,
                                        Exception_state_user *)
  {}

  static void init(Cpu_number cpu, bool resume);

  static void save_state(Fpu_state *s);
  static void restore_state(Fpu_state *s, bool owner);

  static unsigned state_size()
  { return sizeof (Fpu_regs); }

  static unsigned state_align()
  { return 16; }

#if ! defined (CONFIG_CPU_VIRT)
  static void init_state(Fpu_state *s)
  {
    Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
    static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
  }

  static bool is_enabled()
  {
    Mword x;
    asm volatile ("mrs %0, CPACR_EL1" : "=r"(x));
    return x & (3UL << 20);
  }

  static void enable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "orr  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (3UL << 20));
    Mem::isb();
  }

  static void disable()
  {
    Mword t;
    asm volatile("mrs  %0, CPACR_EL1  \n"
                 "bic  %0, %0, %1     \n"
                 "msr  CPACR_EL1, %0  \n"
                 : "=r"(t) : "I" (3UL << 20));
    Mem::isb();
  }

#else // CONFIG_CPU_VIRT

  static void init_state(Fpu_state *s)
  {
    Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
    static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
    //fpu_regs->fpexc |= FPEXC_EN;
  }

  static bool is_enabled()
  {
    Mword dummy;
    __asm__ __volatile__ ("mrs %0, CPTR_EL2" : "=r"(dummy));
    return !(dummy & (1 << 10));
  }


  static void enable()
  {
    Mword dummy;
    __asm__ __volatile__ (
        "mrs %0, CPTR_EL2         \n"
        "bic %0, %0, #(1 << 10)   \n"
        "msr CPTR_EL2, %0         \n"
        : "=&r" (dummy) );
    Mem::isb();
  }

  static void disable()
  {
    Mword dummy;
    __asm__ __volatile__ (
        "mrs  %0, CPTR_EL2           \n"
        "orr  %0, %0, #(1 << 10)     \n"
        "msr  CPTR_EL2, %0           \n"
        : "=&r" (dummy));
    Mem::isb();
  }
#endif // CONFIG_CPU_VIRT
};
