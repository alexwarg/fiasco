#pragma once

#include <entry_frame.h>
#include <cstring>
#include <cp0_status.h>
#include <kobject_iface.h>
#include <mem_unit.h>
#include <trap_state.h>
#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT
#include <cpu.h>
#include <vz.h>
#endif

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    memset(r, 0, sizeof(*r));
    r->status = Cp0_status::status_eret_to_user_ei(Cp0_status::read());
  }

  int sys_control_arch(Utcb const *, Utcb *)
  {
    return 0;
  }

  static void save_fpu_state_to_utcb(Trap_state *, Utcb *)
  {}
};

template<typename THREAD>
class Thread_arch_x : public Thread_arch
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

  int cache_op(unsigned op, Address start, Address end)
  {
    jmp_buf pf_recovery;
    if (setjmp(pf_recovery) != 0)
      {
        _this()->recover_jmp_buf(nullptr);
        return -L4_err::EFault;
      }

    _this()->recover_jmp_buf(&pf_recovery);

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

    _this()->recover_jmp_buf(nullptr);
    return 0;
  }

#ifdef CONFIG_CPU_VIRT
  int sys_vz_save_state(L4_msg_tag tag, Utcb const *utcb)
  {
    if (tag.words() < 2)
      return -L4_err::EMsgtooshort;

    if (!_this()->state.has(Thread_ext_vcpu_enabled))
      return -L4_err::EInval;

    if (current() != _this())
      return -L4_err::EInval;

    auto *v = _this()->vm_state(_this()->vcpu_state().kern());

    // must be saved already
    if (!_this()->state.has(Thread::Thread_vcpu_vz_owner))
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

#else // CONFIG_CPU_VIRT

  int sys_vz_save_state(L4_msg_tag, Utcb const *)
  { return -L4_err::ENosys; }

#endif // CONFIG_CPU_VIRT

protected:
  L4_msg_tag
  invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
  {
    switch (unsigned op = access_once(&utcb->values[0]) & Thread::Opcode_mask)
      {
      case 0x10: // set ULR op-code
        if (tag.words() < 2)
          return Kobject_iface::commit_result(-L4_err::EMsgtooshort);

        _this()->_cpu_state.ulr = access_once(&utcb->values[1]);
        if (current() == _this())
          Proc::set_ulr(_this()->_cpu_state.ulr);
        return Kobject_iface::commit_result(0);

      case 0x14:
        return Kobject_iface::commit_result(sys_vz_save_state(tag, utcb));

      case 0x20:
      case 0x21:
      case 0x22:
        if (tag.words() < 3)
          return Kobject_iface::commit_result(-L4_err::EMsgtooshort);

        return Kobject_iface::commit_result(
            cache_op(op, access_once(&utcb->values[1]),
              access_once(&utcb->values[2])));

      default:
        return Kobject_iface::commit_result(-L4_err::ENosys);
      }
  }

  [[nodiscard]] static bool
  copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
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

    bool ret = Thread::transfer_msg_items(tag, snd, snd_utcb,
                                          rcv, rcv->utcb().access(), rights);

    return ret;
  }

  [[nodiscard]] static bool
  copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    Trap_state *ts = reinterpret_cast<Trap_state*>(snd->_utcb_handler);

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
};

