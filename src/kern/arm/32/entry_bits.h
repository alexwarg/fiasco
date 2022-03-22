#pragma once

namespace Entry {

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
     "r" (r0));
  __builtin_unreachable();
}

}
