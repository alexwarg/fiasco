#pragma once

#include <l4_types.h>
#include <trap_state.h>

template<typename THREAD>
class Thread_arch_bits_x
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

protected:
  static bool FIASCO_WARN_RESULT
  copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                  L4_fpage::Rights rights)
  {
    // only a complete state will be used.
    if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
      return true;

    Trap_state *ts = rcv->utcb_handler_ts();
    Utcb *snd_utcb = snd->utcb().access();

    Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
    // this skips the eret/continuation work already
    rcv->copy_and_sanitize_trap_state(ts, &r->s);
    rcv->set_tpidruro(r);

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

};
