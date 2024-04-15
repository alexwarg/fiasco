#include "mem_layout.h"
#include <arm/32/inline_asm.h>

#ifdef CONFIG_VIRT_OBJ_SPACE
Mword
Mem_layout_arch::_read_special_safe(Mword const *a)
{
  // Counterpart: Thread::pagein_tcb_request()
  register Mword const *res __asm__ ("r14") = a;
  __asm__ __volatile__ (INST32("ldr") " %0, [%0]\n"
                        : "=r" (res) : "r" (res) : "cc" );
  return Mword(res);
}

bool
Mem_layout_arch::_read_special_safe(Mword const *address, Mword &v)
{
  // Counterpart: Thread::pagein_tcb_request()
  register Mword a asm("r14") = reinterpret_cast<Mword>(address);
  Mword ret;
#ifdef __thumb__
  asm volatile ("msr cpsr_f, %[zero]  \n" // clear flags
                "ldr.w %[a], [%[a]]  \n"
                "movne %[ret], #1      \n"
                "moveq %[ret], #0      \n"
                 : [a] "=r" (a), [ret] "=r" (ret)
                 : "0" (a), [zero] "r" (0)
                 : "cc");
#else
  asm volatile ("msr cpsr_f, #0    \n"
                "ldr %[a], [%[a]]  \n"
                "movne %[ret], #1  \n"
                "moveq %[ret], #0  \n"
                : [a] "=r" (a), [ret] "=r" (ret)
                : "0" (a)
                : "cc");
#endif
  v = a;
  return ret;
}
#endif
