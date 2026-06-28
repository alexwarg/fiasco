#pragma once

#include <context.h>

namespace Entry {

[[noreturn]]
inline void reenter_syscall(Context *current)
{
#ifdef CONFIG_CPU_VIRT
  // hvt.S: return one insn before fast_ret_from_irq (to add sp, sp, #8)
  // hvt.S: sp is 8 (2*4) below regs
  static constexpr int lr_offset = -4;
  static constexpr int sp_offset = -2;
#else
  // hvt.S: return one insn after fast_ret_from_irq (to 2: ldmia...)
  // hvt.S: sp is at regs
  static constexpr int lr_offset = +4;
  static constexpr int sp_offset = 0;
#endif
  extern char fast_ret_from_irq[] asm ("fast_ret_from_irq");
  register Mword lr asm("lr") = reinterpret_cast<Mword>(fast_ret_from_irq) +lr_offset;
  asm volatile ("mov sp, %0\n\t"
                "b sys_ipc_wrapper"
                : : "r"(reinterpret_cast<Mword *>(current->regs()) + sp_offset),
                    "r"(lr)
                : "memory");
  __builtin_unreachable();
}

[[noreturn]]
inline void arm_fast_exit(void *sp, void *pc, void *arg)
{
  register void *r0 asm("r0") = arg;
  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  bx      %[rfe]        \n"
     : :
     [stack_p] "r" (sp),
     [rfe]     "r" (pc),
     "r" (r0)
     : "memory");
  __builtin_unreachable();
}

}
