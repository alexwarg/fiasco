#include "mem_layout.h"

Mword
Mem_layout_arch::_read_special_safe(Mword const *a)
{
  // Workaround GCC BUG 33661
  // Do not use register asm ("r") in a template function, it will be ignored
  register Mword const *res __asm__ ("r14") = a;
  __asm__ __volatile__ ("ldr %0, [%0]\n" : "=r" (res) : "r" (res) : "cc");
  return Mword(res);
}

bool
Mem_layout_arch::_read_special_safe(Mword const *address, Mword &v)
{
  // Workaround GCC BUG 33661
  // Do not use register asm ("r") in a template function, it will be ignored
  register Mword a asm("r14") = (Mword)address;
  Mword ret;
  asm volatile ("msr cpsr_f, #0    \n"
                "ldr %[a], [%[a]]  \n"
                "movne %[ret], #1  \n"
                "moveq %[ret], #0  \n"
                : [a] "=r" (a), [ret] "=r" (ret)
                : "0" (a)
                : "cc");
  v = a;
  return ret;
}
