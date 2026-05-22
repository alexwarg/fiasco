#pragma once
// ARM 32-bit Proc_arm_bits template — bitness-specific Proc members.
// Included by drivers/arm/processor-arch.h.

#include <globalconfig.h>
#include "std_macros.h"
#include "types.h"

#ifdef CONFIG_ARM_EM_TZ
# define ARM_CPS_INTERRUPT_FLAGS "f"
#else
# define ARM_CPS_INTERRUPT_FLAGS "if"
#endif

template<typename DERIVED>
class Proc_arm_bits
{
public:
  typedef Mword Status;

#ifndef CONFIG_CPU_VIRT
  enum : unsigned
  {
    // user threads run on 'usr' mode
    Status_mode_user      = 0x10, // usr
    Status_mode_always_on = 0x110,
  };
#else
  enum : unsigned
  {
    // user threads on a hyp kernel run in system mode
    Status_mode_user      = 0x1f, // sys
    Status_mode_always_on = 0x110,
  };
#endif

#ifdef CONFIG_ARM_V6PLUS
  static inline void cli()
  {
    asm volatile("cpsid " ARM_CPS_INTERRUPT_FLAGS : : : "memory");
  }

  static inline ALWAYS_INLINE void sti()
  {
    asm volatile("cpsie " ARM_CPS_INTERRUPT_FLAGS : : : "memory");
  }

  static inline ALWAYS_INLINE Status cli_save()
  {
    Status prev;
    asm volatile("mrs %0, cpsr \n"
                 "cpsid " ARM_CPS_INTERRUPT_FLAGS
                 : "=r" (prev)
                 :
                 : "memory");
    return prev;
  }
#else // !CONFIG_ARM_V6PLUS
  static inline void cli()
  {
    Mword v;
    asm volatile("mrs %0, cpsr    \n"
                 "orr %0, %0, %1  \n"
                 "msr cpsr_c, %0  \n"
                 : "=r" (v)
                 : "I" (DERIVED::Cli_mask)
                 : "memory");
  }

  static inline ALWAYS_INLINE void sti()
  {
    Mword v;
    asm volatile("mrs %0, cpsr    \n"
                 "bic %0, %0, %1  \n"
                 "msr cpsr_c, %0  \n"
                 : "=r" (v)
                 : "I" (DERIVED::Sti_mask)
                 : "memory");
  }

  static inline ALWAYS_INLINE Status cli_save()
  {
    Status ret;
    Mword v;
    asm volatile("mrs %0, cpsr    \n"
                 "orr %1, %0, %2  \n"
                 "msr cpsr_c, %1  \n"
                 : "=r" (ret), "=r" (v)
                 : "I" (DERIVED::Cli_mask)
                 : "memory");
    return ret;
  }
#endif // CONFIG_ARM_V6PLUS

  static inline Status interrupts()
  {
    Status ret;
    asm volatile("mrs %0, cpsr" : "=r" (ret));
    return !(ret & DERIVED::Sti_mask);
  }

  static inline Mword program_counter()
  {
    Mword pc;
    asm ("mov %0, pc" : "=r" (pc));
    return pc;
  }

#if !defined(CONFIG_ARM_V7) && !defined(CONFIG_ARM_V8)
  static inline Cpu_phys_id cpu_id()
  { return Cpu_phys_id(0); }
#else
  static inline Cpu_phys_id cpu_id()
  {
    unsigned mpidr;
    __asm__("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
    return Cpu_phys_id(mpidr & 0xffffff);
  }
#endif

#if defined(CONFIG_ARM_V7) || defined(CONFIG_ARM_V8)
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
#elif defined(CONFIG_ARM_PXA) || defined(CONFIG_ARM_SA) || defined(CONFIG_ARM_920T)
  static inline void pause()
  {}

  static inline void halt()
  {}
#elif defined(CONFIG_ARM_926)
  static inline void pause()
  {}

  static inline void halt()
  {
    Status f = DERIVED::cli_save();
    asm volatile("mov     r0, #0                                              \n\t"
                 "mrc     p15, 0, r1, c1, c0, 0       @ Read control register \n\t"
                 "mcr     p15, 0, r0, c7, c10, 4      @ Drain write buffer    \n\t"
                 "bic     r2, r1, #1 << 12                                    \n\t"
                 "mcr     p15, 0, r2, c1, c0, 0       @ Disable I cache       \n\t"
                 "mcr     p15, 0, r0, c7, c0, 4       @ Wait for interrupt    \n\t"
                 "mcr     15, 0, r1, c1, c0, 0        @ Restore ICache enable \n\t"
                 :::"memory",
                 "r0", "r1", "r2", "r3", "r4", "r5",
                 "r6", "r7", "r8", "r9", "r10", /* r11: fp */
                 "r12", "r14"
        );
    DERIVED::sti_restore(f);
  }
#elif defined(CONFIG_ARM_1136)
  static inline void pause()
  {}

  static inline void halt()
  {
    Status f = DERIVED::cli_save();
    asm volatile("mcr     p15, 0, r0, c7, c10, 4  @ DWB/DSB \n\t"
                 "mcr     p15, 0, r0, c7, c0, 4   @ WFI \n\t");
    DERIVED::sti_restore(f);
  }
#elif defined(CONFIG_ARM_1176) || defined(CONFIG_ARM_MPCORE)
  static inline void pause()
  {}

  static inline void halt()
  {
    Status f = DERIVED::cli_save();
    asm volatile("mcr     p15, 0, r0, c7, c10, 4  @ DWB/DSB \n\t"
                 "wfi \n\t");
    DERIVED::sti_restore(f);
  }
#endif
};
