#pragma once

#include "kernel_thread.h"

inline void
main_switch_ap_cpu_stack(Kernel_thread *kernel, bool resume)
{
  Mword dummy;

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	mov %[rsp], %%rsp	\n\t"	// switch stack
     "	call call_ap_bootstrap	\n\t"	// bootstrap kernel thread
     :  "=a" (dummy), "=c" (dummy), "=d" (dummy)
     :	"D"(kernel), "S"(resume), [rsp]"r" (kernel->init_stack()));
}

