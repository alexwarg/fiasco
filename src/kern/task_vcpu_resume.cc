
#include "task.h"

#include "std_macros.h"
#include "trap_state.h"
#include "entry_frame.h"

extern "C" void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL FIASCO_NORETURN;

int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  // BAD: use the top-of the context stack area for the vcpu_resume
  // return, otherwise exceptions during return to user are very
  // ugly to handle.
  Trap_state *ts = reinterpret_cast<Trap_state *>(ctxt->regs() + 1) - 1;
  ctxt->copy_and_sanitize_trap_state(ts, &vcpu->_regs.s);

  if (user_mode)
    {
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps | Vcpu_state::F_exceptions;

      ctxt->vcpu_pv_switch_to_user(vcpu, true);
    }

  ctxt->space_ref()->user_mode(user_mode);
  switchin_context(ctxt->space());
  vcpu_resume(ts, ctxt->regs());
}


