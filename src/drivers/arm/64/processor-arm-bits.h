#pragma once
// ARM 64-bit Proc_arm_bits template — bitness-specific Proc members.
// Included by drivers/arm/processor-arch.h.

#include <globalconfig.h>
#include "std_macros.h"
#include "types.h"

template<typename DERIVED>
class Proc_arm_bits
{
public:
  typedef Mword Status;

#ifndef CONFIG_CPU_VIRT
  enum : unsigned
  {
    Status_mode_user      = 0x00,
    Status_mode_always_on = 0x100,
  };
#else
  enum : unsigned
  {
    // user threads on a hyp kernel run in system mode
    Status_mode_user      = 0x04, // EL1t
    Status_mode_always_on = 0x100,
  };
#endif

  static inline void cli()
  {
    asm volatile("msr DAIFSet, %0" : : "i"(DERIVED::Cli_mask >> 6) : "memory");
  }

  static inline ALWAYS_INLINE void sti()
  {
    asm volatile("msr DAIFClr, %0" : : "i"(DERIVED::Cli_mask >> 6) : "memory");
  }

  static inline ALWAYS_INLINE Status cli_save()
  {
    Status prev;
    asm volatile("mrs %0, DAIF \n"
                 "msr DAIFSet, %1"
                 : "=r" (prev)
                 : "i"(DERIVED::Cli_mask >> 6)
                 : "memory");
    return prev;
  }

  static inline void pause()
  {
    asm("yield");
  }

  static inline void halt()
  {
    Status f = DERIVED::cli_save();
    asm volatile("dsb sy \n\t"
                 "wfi \n\t");
    DERIVED::sti_restore(f);
  }

  static inline Status interrupts()
  {
    Status ret;
    asm volatile("mrs %0, DAIF" : "=r" (ret));
    return !(ret & DERIVED::Sti_mask);
  }

  static inline ALWAYS_INLINE Mword program_counter()
  {
    Mword pc;
    asm ("ldr %0, =1f; 1:" : "=r" (pc));
    return pc;
  }

  static inline Cpu_phys_id cpu_id()
  {
    Mword mpidr;
    __asm__("mrs %0, MPIDR_EL1" : "=r" (mpidr));
    return Cpu_phys_id((mpidr & 0xffffff) | ((mpidr >> 8) & 0xff000000));
  }

#ifndef CONFIG_CPU_VIRT
  static inline Unsigned32 sctlr()
  {
    Mword v;
    asm volatile ("mrs %0, SCTLR_EL1" : "=r"(v));
    return v;
  }
#else
  static inline Unsigned32 sctlr_el1()
  {
    Mword v;
    asm volatile ("mrs %0, SCTLR_EL1" : "=r"(v));
    return v;
  }

  static inline Unsigned32 sctlr()
  {
    Mword v;
    asm volatile ("mrs %0, SCTLR_EL2" : "=r"(v));
    return v;
  }
#endif
};
