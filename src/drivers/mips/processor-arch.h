#pragma once
// MIPS Proc_arch<DERIVED>

#include <globalconfig.h>
#include <mips_cpu_regs.h>
#include "std_macros.h"
#include "types.h"
#include "warn.h"
#include "cp0_status.h"
#include "asm_mips.h"
#include "alternatives.h"

template<typename DERIVED>
class Proc_arch
{
public:
  typedef Mword Status;

  static inline ALWAYS_INLINE void sti()
  {
    asm volatile (
      "ei   \n"
      "ehb  \n"
      : /* no outputs */
      : /* no inputs */
      : "memory"
    );
  }

  static inline void cli()
  {
    asm volatile (
      "di   \n"
      "ehb  \n"
      : /* no outputs */
      : /* no inputs */
      : "memory"
    );
  }

  static inline Status interrupts()
  {
    return (Status)Cp0_status::read() & Cp0_status::ST_IE;
  }

  static inline Status interrupts(Status status)
  {
    return status & Cp0_status::ST_IE;
  }

  static inline ALWAYS_INLINE Status cli_save()
  {
    Status flags;
    asm volatile (
      "di   %[flags]\n"
      "ehb  \n"
      : [flags] "=r" (flags)
      : /* no inputs */
      : "memory"
    );
    return flags & Cp0_status::ST_IE;
  }

  static inline ALWAYS_INLINE void sti_restore(Status status)
  {
    if (status & Cp0_status::ST_IE)
      DERIVED::sti();
  }

  static inline void pause()
  {
    // FIXME: could use 'rp' here?
  }

  static inline void halt()
  {
    asm volatile ("wait");
  }

  static inline void irq_chance()
  {
    asm volatile ("nop; nop;" : : : "memory");
  }

  static inline void stack_pointer(Mword sp)
  {
    asm volatile ("move $29,%0" : : "r" (sp));
  }

  static inline Mword stack_pointer()
  {
    Mword sp;
    asm volatile ("move %0,$29" : "=r" (sp));
    return sp;
  }

  static inline Mword program_counter()
  {
    Mword pc;
    asm volatile (
        "move  $t0, $ra  \n"
        "jal   1f        \n"
        "1:              \n"
        "move  %0, $ra   \n"
        "move  $ra, $t0  \n"
        : "=r" (pc) : : "t0");
    return pc;
  }

  static inline void cp0_exec_hazard()
  { __asm__ __volatile__ ("ehb"); }

  static inline Mword get_ulr()
  {
    Mword v;
    __asm__ __volatile__ (ASM_MFC0 " %0, $4, 2" : "=r"(v));
    return v;
  }

  static inline void set_ulr(Mword ulr)
  {
    __asm__ __volatile__ (ALTERNATIVE_INSN(
          "nop",
          ASM_MTC0 " %0, $4, 2", /* load ULR if it is supported */
          0x00000002              /* feature bit 1 (see Cpu::Options ulr) */
          )
        : : "r"(ulr));
  }

#ifndef CONFIG_MP
  static inline Cpu_phys_id cpu_id()
  { return Cpu_phys_id(0); }
#else
  static inline Cpu_phys_id cpu_id()
  {
    Mword v;
    asm volatile ("mfc0 %0, $15, 1" : "=r"(v));
    return Cpu_phys_id(v & 0x3ff);
  }
#endif
};
