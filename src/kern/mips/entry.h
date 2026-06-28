#pragma once

#include <cp0_status.h>
#include <context.h>

namespace Entry {

[[noreturn]]
inline void reenter_syscall(Context *current)
{
  extern char sys_ipc_call_patch[] asm ("sys_ipc_call_patch");
#ifdef __mips64
  static constexpr int sp_offset = 0;
#else
  static constexpr int sp_offset = 4 * 4;
#endif
  register Mword lr asm("ra") = reinterpret_cast<Mword>(&sys_ipc_call_patch) + 8;
  asm volatile
      (".set push                     \n"
       ".set noat                     \n"
       "move $29, %0 \n"
       "j sys_ipc_wrapper \n"
       ".set pop                      \n"
       : : "r"(reinterpret_cast<char *>(current->regs()) - sp_offset),
           "r"(lr)
       : "memory");
  __builtin_unreachable();
}


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
       : "memory"
      );
  }

  __builtin_unreachable();
}


}

