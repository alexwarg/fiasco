#pragma once

#include <entry_bits.h>
#include <context.h>

namespace Entry {

[[noreturn]] inline void
vcpu_return_to_kernel(Context *c, Mword ip, Mword sp, Vcpu_state *arg)
{
  extern char __iret[] asm ("__iret");

  Entry_frame *r = c->regs();

  r->ip(ip);
  r->sp(sp); // user-sp is in lazy user state and thus handled by
             // fill_user_state()
  c->fill_user_state();
  //load_tpidruro();

  // masking the illegal execution bit does not harm
  // on 32bit it is res/sbz
  r->psr &= ~(Proc::Status_thumb | (1UL << 20));

  // make sure the VMM executes in the correct mode
  if (Proc::Is_hyp)
    {
      r->psr_set_mode(Proc::Status_mode_user);
      r->psr |= 0x1c0; // mask PSTATE.{I,A,F}
    }

  assert(r->check_valid_user_psr());
  Entry::arm_fast_exit(nonull_static_cast<Return_frame*>(r), __iret, arg);

  // never returns here
}



}
