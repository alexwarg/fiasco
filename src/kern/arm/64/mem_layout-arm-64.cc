#include "mem_layout.h"
#include <cpu.h>
#include <paging.h>

#ifdef CONFIG_VIRT_OBJ_SPACE
Mword
Mem_layout_arch::_read_special_safe(Mword const *a)
{
  // Counterpart: Thread::pagein_tcb_request()
  Mword res;
  __asm__ __volatile__ ("ldr %0, %1\n" : "=r" (res) : "m" (*a) : "cc");
  return res;
}

bool
Mem_layout_arch::_read_special_safe(Mword const *address, Mword &v)
{
  // Counterpart: Thread::pagein_tcb_request()
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
#endif
