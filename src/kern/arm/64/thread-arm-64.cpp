IMPLEMENTATION [arm && 64bit]:

#include "fpu.h"
#include "slowtrap_entry.h"

PUBLIC static inline void FIASCO_NORETURN
Thread::arm_fast_exit(void *sp, void *pc, void *arg)
{
  register void *r0 asm("x0") = arg;
  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  br  %[rfe]            \n"
     : :
     [stack_p] "r" (sp),
     [rfe]     "r" (pc),
     "r" (r0));
  __builtin_unreachable();
}


extern "C" void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  Mem::memcpy_mwords(vcpu->_regs.s.r, ts->r, 31);
  vcpu->_regs.s.usp = ts->usp;
  vcpu->_regs.s.pc = ts->pc;
  vcpu->_regs.s.pstate = ts->pstate;
  c->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_sp, c->vcpu_state().usr().get());
}

PROTECTED static inline
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  // this skips the eret/continuation work already
  rcv->copy_and_sanitize_trap_state(ts, &r->s);
  rcv->set_tpidruro(r);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PROTECTED static inline NEEDS[Thread::store_tpidruro,
                            "trap_state.h"]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;
    snd->store_tpidruro(r);

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

