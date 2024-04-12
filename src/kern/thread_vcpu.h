#pragma once

#include <context.h>
#include <vcpu_log.h>
#include <logdefs.h>

class Thread_vcpu
{
public:
  static bool
  vcpu_pagefault(Context *c, Address pfa, Mword err, Mword ip)
  {
    (void)ip;
    Vcpu_state *vcpu = c->vcpu_state().access();
    if (!c->vcpu_pagefaults_enabled(vcpu))
      return false;

    c->spill_user_state();
    c->vcpu_enter_kernel_mode(vcpu);
    LOG_TRACE("VCPU events", "vcpu", c, Vcpu_log,
        l->type = 3;
        l->state = vcpu->saved_state();
        l->ip = ip;
        l->sp = pfa;
        l->err = err;
        l->space = c->vcpu_user_space()
                   ? static_cast<Task*>(c->vcpu_user_space())->dbg_id()
                   : ~0;
        );
    vcpu->_regs.s.set_pagefault(pfa, err);
    c->vcpu_save_state_and_upcall();
    return true;
  }
};
