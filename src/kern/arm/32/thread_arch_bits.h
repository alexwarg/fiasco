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
  [[nodiscard]] static bool
  copy_utcb_to_ts(L4_msg_tag tag, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    // if the message is too short just skip the whole copy in
    if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
      return true;

    Trap_state *ts = rcv->utcb_handler_ts();
    Utcb *snd_utcb = snd->utcb().access();
    Trex const *sregs = reinterpret_cast<Trex const *>(snd_utcb->values);

    if (EXPECT_FALSE(rcv->exception_triggered()))
      {
        // triggered exception pending -- copy pf_address, esr, r0..r12
        Mem::memcpy_mwords(ts, snd_utcb->values, 15);
        Return_frame rf = access_once(static_cast<Return_frame const *>(&sregs->s));
        rcv->sanitize_user_state(&rf);
        rcv->cont()->set(ts, &rf);
      }
    else
      rcv->copy_and_sanitize_trap_state(ts, &sregs->s);

    if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    // FIXME: this is an old l4linux specific hack, will be replaced/remved
    if ((tag.flags() & 0x8000) && (rights & L4_fpage::Rights::CS()))
      rcv->utcb().access()->user[2] = snd_utcb->values[25];

    rcv->set_tpidruro(sregs);

    bool ret = Thread::transfer_msg_items(tag, snd, snd_utcb,
                                          rcv, rcv->utcb().access(), rights);

    return ret;
  }


  [[nodiscard]] static bool
  copy_ts_to_utcb(L4_msg_tag, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    Trap_state *ts = snd->utcb_handler_ts();

    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *rregs = reinterpret_cast<Trex *>(rcv_utcb->values);

    snd->store_tpidruro(rregs);

    // copy pf_address, esr, r0..r12
    Mem::memcpy_mwords(rcv_utcb->values, ts, 15);
    Continuation::User_return_frame *d
      = reinterpret_cast<Continuation::User_return_frame *>((char*)&rcv_utcb->values[15]);

    snd->cont()->get(d, ts);


    if (EXPECT_TRUE(!snd->exception_triggered()))
      {
        rcv_utcb->values[18] = ts->pc;
        rcv_utcb->values[19] = ts->psr;
      }

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      {
        snd->save_fpu_state_to_utcb(ts, rcv_utcb);
        snd->transfer_fpu(rcv);
      }

    return true;
  }

#ifdef CONFIG_ARM_V6PLUS
  void vcpu_resume_user_arch()
  {
    // just an experiment for now, we cannot really take the
    // user-writable register because user-land might already use it
    asm volatile("mcr p15, 0, %0, c13, c0, 2"
                 : : "r" (_this()->utcb().access(true)->values[25]) : "memory");
  }
#else // CONFIG_ARM_V6PLUS
  void vcpu_resume_user_arch()
  {}
#endif // CONFIG_ARM_V6PLUS
};
