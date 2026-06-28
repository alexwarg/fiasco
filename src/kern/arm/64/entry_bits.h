#pragma once

#include <context.h>

namespace Entry {

[[noreturn]]
inline void reenter_syscall(Context *current)
{
  extern char __return_from_syscall_fn[] asm ("__return_from_syscall_fn");
  register Mword x30 asm("x30") = reinterpret_cast<Mword>(__return_from_syscall_fn);
  asm volatile ("mov sp, %0\n\t"
                "b sys_ipc_wrapper"
                : : "r"(reinterpret_cast<Mword *>(current->regs())),
                    "r"(x30)
                : "memory");
  __builtin_unreachable();
}

[[noreturn]]
inline void arm_fast_exit(void *sp, void *pc, void *arg)
{
  register void *r0 asm("x0") = arg;
  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  br  %[rfe]            \n"
     : :
     [stack_p] "r" (sp),
     [rfe]     "r" (pc),
     "r" (r0)
     : "memory");
  __builtin_unreachable();
}

}
