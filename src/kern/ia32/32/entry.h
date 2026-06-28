#pragma once

#include <cassert>
#include <cpu.h>
#include <globalconfig.h>
#include <context.h>

namespace Entry {

[[noreturn]]
inline void reenter_syscall(Context *current)
{
  asm volatile ("mov %0, %%esp\n\t"
                "jmp sys_ipc_wrapper"
                : : "r"(reinterpret_cast<Mword *>(current->regs()) - 1)
                : "memory");
  __builtin_unreachable();
}

template<typename T>
[[noreturn]] inline void vcpu_return_to_kernel(Context *c, Mword ip, Mword sp, T arg)
{
  assert(cpu_lock.test());

  Return_frame *regs = c->regs();
  assert((regs->cs() & 3) == 3);

  regs->ip(ip);
  regs->sp(sp);
  regs->flags(EFLAGS_IF);
  asm volatile
    ("mov %0, %%esp \t\n"
     "iret         \t\n"
     :
     : "r" (regs), "a" (arg)
     : "memory"
    );
  __builtin_unreachable();
}

}

