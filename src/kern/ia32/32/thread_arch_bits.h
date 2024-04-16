#pragma once

#include <l4_types.h>

template<typename THREAD>
class Thread_arch_bits_x
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

protected:
  L4_msg_tag
  invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *out)
  {
    switch (utcb->values[0] & Thread::Opcode_mask)
      {
      case Thread::Op_gdt_x86: return _this()->sys_gdt_x86(tag, utcb, out);
      default:
        return Kobject_iface::commit_result(-L4_err::ENosys);
      };
  }

  static bool FIASCO_WARN_RESULT
  copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    if (EXPECT_FALSE((tag.words() * sizeof(Mword)) < sizeof(Trex)))
      return true;

    Trap_state *ts = rcv->utcb_handler_ts();
    Unsigned32  cs = ts->cs();
    Utcb *snd_utcb = snd->utcb().access();
    Trex const *src = reinterpret_cast<Trex const *>(snd_utcb->values);

    // XXX: check that gs and fs point to valid user_entry only, for gdt and
    // ldt!
    if (EXPECT_FALSE(rcv->exception_triggered()))
      {
        // triggered exception pending, skip ip, cs, flags, and sp
        Mem::memcpy_mwords(ts, &src->s, Ts::Reg_words);
        Continuation::User_return_frame const *urfp
          = reinterpret_cast<Continuation::User_return_frame const *>(
              (char*)&src->s._ip);

        Continuation::User_return_frame urf = access_once(urfp);

        // sanitize flags
        urf.flags(Thread::sanitize_user_flags(urf.flags()));
        rcv->cont()->set(Thread::trap_state_to_rf(ts), &urf);
      }
    else
      {
        Mem::memcpy_mwords(ts, &src->s, Ts::Words);
        // sanitize flags
        ts->flags(Thread::sanitize_user_flags(ts->flags()));
        // don't allow to overwrite the code selector!
        ts->cs(cs);
      }

    auto &rcs = rcv->_cpu_state_();
    // reset segments
    rcs.gs = rcs.fs = 0;

    if (rcv == current())
      rcs.gdt_user_entries.load();

    if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    bool ret = Thread::transfer_msg_items(tag, snd, snd_utcb,
                                          rcv, rcv->utcb().access(), rights);

    return ret;
  }

  static bool FIASCO_WARN_RESULT
  copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    Trap_state *ts = snd->utcb_handler_ts();
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *dst = reinterpret_cast<Trex *>(rcv_utcb->values);
      {
        auto guard = lock_guard(cpu_lock);
        if (EXPECT_FALSE(snd->exception_triggered()))
          {
            Mem::memcpy_mwords(&dst->s, ts, Ts::Reg_words + Ts::Code_words);
            Continuation::User_return_frame *d
              = reinterpret_cast<Continuation::User_return_frame *>(
                  (char*)&dst->s._ip);

            snd->cont()->get(d, Thread::trap_state_to_rf(ts));
          }
        else
          Mem::memcpy_mwords(&dst->s, ts, Ts::Words);

        if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
          snd->transfer_fpu(rcv);
      }
    return true;
  }


};
