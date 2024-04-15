IMPLEMENTATION [mips]:

#include <cstdio>
#include <cassert>
#include "alternatives.h"
#include "asm_mips.h"
#include "cp0_status.h"
#include "kip.h"
#include "trap_state.h"
#include "processor.h"
#include "types.h"

#include <thread_vcpu.h>

PROTECTED inline
int
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

PRIVATE inline
int
Thread::cache_op(unsigned op, Address start, Address end)
{
  jmp_buf pf_recovery;
  if (setjmp(pf_recovery) != 0)
    {
      recover_jmp_buf(0);
      return -L4_err::EFault;
    }

  recover_jmp_buf(&pf_recovery);

  switch (op)
    {
    case 0x22:
      // this is invalidate only, however we do a flush
      // otherwise we would need to do a flush for incomplete
      // cachelines at the start end end of the range
    case 0x20:
      Mem_unit::dcache_flush(start, end);
      break;
    case 0x21:
      Mem_unit::dcache_clean(start, end);
      break;
    }

  recover_jmp_buf(0);
  return 0;
}


PROTECTED inline NEEDS["processor.h", Thread::sys_vz_save_state,
                       Thread::cache_op]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
{
  switch (unsigned op = access_once(&utcb->values[0]) & Opcode_mask)
    {
    case 0x10: // set ULR op-code
      if (tag.words() < 2)
        return commit_result(-L4_err::EMsgtooshort);

      _cpu_state.ulr = access_once(&utcb->values[1]);
      if (current() == this)
        Proc::set_ulr(_cpu_state.ulr);
      return Kobject_iface::commit_result(0);

    case 0x14:
      return commit_result(sys_vz_save_state(tag, utcb));

    case 0x20:
    case 0x21:
    case 0x22:
      if (tag.words() < 3)
        return commit_result(-L4_err::EMsgtooshort);

      return commit_result(cache_op(op, access_once(&utcb->values[1]),
                                    access_once(&utcb->values[2])));

    default:
      return commit_result(-L4_err::ENosys);
    }
}

PROTECTED inline
int
Thread::do_trigger_exception(Entry_frame *r, void *ret_handler)
{
  if (!_exc_cont.valid(r))
    {
      _exc_cont.activate(r, ret_handler);
      return 1;
    }
  return 0;
}

PROTECTED static inline
void
Thread::save_fpu_state_to_utcb(Trap_state *, Utcb *)
{}

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
  ts->copy_and_sanitize(&r->s);
  rcv->_cpu_state.ulr = access_once(&r->ulr);
  if (rcv == current())
    Proc::set_ulr(rcv->_cpu_state.ulr);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PROTECTED static inline NEEDS["trap_state.h"]
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
    r->ulr = snd->_cpu_state.ulr;

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

//----------------------------------------------------------
IMPLEMENTATION [mips && !mips_vz]:

PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag, Utcb const *)
{ return -L4_err::ENosys; }

//----------------------------------------------------------
IMPLEMENTATION [mips && mips_vz]:

#include "cpu.h"
#include "vz.h"
PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag tag, Utcb const *utcb)
{
  if (tag.words() < 2)
    return -L4_err::EMsgtooshort;

  if (!(state() & Thread_ext_vcpu_enabled))
    return -L4_err::EInval;

  if (current() != this)
    return -L4_err::EInval;

  auto *v = vm_state(vcpu_state().kern());

  // must be saved already
  if (!(state() & Thread_vcpu_vz_owner))
    {
      // update the cause TI bit in this case
      if (utcb->values[1] & Vz::State::M_cause)
        v->update_cause_ti();
      return 0;
    }

  // always read the cause register when requested by the VMM
  if (utcb->values[1] & Vz::State::M_cause)
    v->current_cp0_map &= ~Vz::State::M_cause;

  // we have a bitmap in utcb->values[1] which state to save, however
  // we save the full state for now.
  v->save_full(Vz::owner.current().guest_id);
  return 0;
}


