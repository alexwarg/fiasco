#pragma once

#include <cp0_status.h>

class Context;

namespace Entry {

[[noreturn]] inline void
vcpu_return_to_kernel(Context *, Mword ip, Mword sp, void *arg)
{
  assert (cpu_lock.test());

  {
    register void *a0 __asm__("a0") = arg;
    register Mword t9 __asm__("t9") = ip;
    asm volatile
      (".set push                     \n"
       ".set noat                     \n"
       "  mfc0  $1, $12               \n"
       "  ins   $1, %[status], 0, 8   \n"
       "  move  $29, %[sp]            \n"
       "  " ASM_MTC0 "  %[ip], $14    \n"
       "  mtc0  $1, $12               \n"
       "  ehb                         \n"
       "  eret                        \n"
       ".set pop                      \n"
       : : [status] "r" (Cp0_status::ST_USER_DEFAULT),
           [ip] "r" (t9), [sp] "r" (sp), [arg] "r" (a0)
      );
  }

  __builtin_unreachable();
}


}

