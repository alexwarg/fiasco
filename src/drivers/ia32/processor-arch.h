#pragma once
// ia32 Proc_arch<DERIVED>

#include "types.h"
#include "std_macros.h"

template<typename DERIVED>
class Proc_arch
{
public:
  typedef Mword Status;

  /**
   * Return CPU ID.
   *
   * Depending on the platform, this is either a 6-bit, an 8-bit, or a 32-bit
   * value. Note that the 32-bit x2APIC CPU ID requires 2 invocations of CPUID.
   */
  static inline Cpu_phys_id cpu_id()
  {
    Unsigned32 eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (eax >= 0xb)
      return Cpu_phys_id(cpuid_edx(0xb));
    else
      return Cpu_phys_id(ebx >> 24);
  }

  static inline Mword stack_pointer()
  {
    Mword sp;
    asm volatile ("movl %%esp, %0" : "=r" (sp));
    return sp;
  }

  static inline Mword stack_pointer_for_context()
  {
    Mword sp;
    asm ("movl %%esp, %0" : "=r" (sp));
    return sp;
  }

  static inline void stack_pointer(Mword sp)
  {
    asm volatile ("movl %0, %%esp" : : "r" (sp));
  }

  static inline Mword program_counter()
  {
    Mword pc;
    asm volatile ("call 1f; 1: pop %0" : "=r" (pc));
    return pc;
  }

  static inline void pause()
  {
    asm volatile ("pause");
  }

  static inline void halt()
  {
    asm volatile ("hlt");
  }

  static inline void cli()
  {
    asm volatile ("cli" : : : "memory");
  }

  static inline void sti()
  {
    asm volatile ("sti" : : : "memory");
  }

  static inline Status cli_save()
  {
    Status ret;
    asm volatile ("pushfl   \n\t"
                  "popl %0  \n\t"
                  "cli      \n\t"
                  : "=g" (ret) : /* no input */ : "memory");
    return ret;
  }

  static inline void sti_restore(Status st)
  {
    if (st & 0x0200)
      asm volatile ("sti" : : : "memory");
  }

  static inline Status interrupts()
  {
    Status ret;
    asm volatile ("pushfl    \n\t"
                  "popl %0   \n\t"
                  : "=g" (ret) : /* no input */ : "memory");
    return ret & 0x0200;
  }

  static inline void irq_chance()
  {
    asm volatile ("nop");
    DERIVED::pause();
  }

  static inline Unsigned64 rdmsr(Unsigned32 msr)
  {
    Unsigned64 v;
    asm volatile ("rdmsr" : "=A" (v) : "c" (msr));
    return v;
  }

  static inline void wrmsr(Unsigned64 value, Unsigned32 msr)
  {
    asm volatile ("wrmsr": :
                  "a" ((Unsigned32)value),
                  "d" ((Unsigned32)(value >> 32)),
                  "c" (msr));
  }

  static inline void cpuid(Unsigned32 mode, Unsigned32 ecx_val,
                           Unsigned32 *eax, Unsigned32 *ebx,
                           Unsigned32 *ecx, Unsigned32 *edx)
  {
    asm volatile ("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                          : "a" (mode), "c" (ecx_val));
  }

  static inline void cpuid(Unsigned32 mode,
                           Unsigned32 *eax, Unsigned32 *ebx,
                           Unsigned32 *ecx, Unsigned32 *edx)
  { DERIVED::cpuid(mode, 0, eax, ebx, ecx, edx); }

  static inline Unsigned32 cpuid_eax(Unsigned32 mode)
  {
    Unsigned32 eax, dummy;
    DERIVED::cpuid(mode, &eax, &dummy, &dummy, &dummy);
    return eax;
  }

  static inline Unsigned32 cpuid_ebx(Unsigned32 mode)
  {
    Unsigned32 ebx, dummy;
    DERIVED::cpuid(mode, &dummy, &ebx, &dummy, &dummy);
    return ebx;
  }

  static inline Unsigned32 cpuid_ecx(Unsigned32 mode)
  {
    Unsigned32 ecx, dummy;
    DERIVED::cpuid(mode, &dummy, &dummy, &ecx, &dummy);
    return ecx;
  }

  static inline Unsigned32 cpuid_edx(Unsigned32 mode)
  {
    Unsigned32 edx, dummy;
    DERIVED::cpuid(mode, &dummy, &dummy, &dummy, &edx);
    return edx;
  }
};
