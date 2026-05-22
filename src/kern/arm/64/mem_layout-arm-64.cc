#include "mem_layout.h"

Mword
Mem_layout_arch::_read_special_safe(Mword const *a)
{
  // Workaround GCC BUG 33661
  // Do not use register asm ("r") in a template function, it will be ignored
  Mword res;
  __asm__ __volatile__ ("ldr %0, %1\n" : "=r" (res) : "m" (*a) : "cc");
  return res;
}

bool
Mem_layout_arch::_read_special_safe(Mword const *address, Mword &v)
{
  // Workaround GCC BUG 33661
  // Do not use register asm ("r") in a template function, it will be ignored
  Mword ret;
  asm volatile ("msr  nzcv, xzr      \n"
                "mov  %[ret], #1     \n"
                "ldr  %[val], %[adr] \n"
                "b.ne 1f             \n"
                "mov  %[ret], xzr    \n"
                "1:                  \n"
                : [val] "=r" (v), [ret] "=&r" (ret)
                : [adr] "m" (*address)
                : "cc");
  return ret;
}
