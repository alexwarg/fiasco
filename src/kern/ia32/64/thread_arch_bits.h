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
      case Thread::Op_gdt_x86:
        return _this()->sys_gdt_x86(tag, utcb, out);

      case Thread::Op_set_segment_base_amd64:
        {
          if (tag.words() < 2)
            return Kobject_iface::commit_result(-L4_err::EMsgtooshort);

          Mword base = access_once(utcb->values + 1);
          if (!Cpu::is_canonical_address(base))
            return Kobject_iface::commit_result(-L4_err::EInval);

          auto &s = _this()->_cpu_state_();
          switch (utcb->values[0] >> 16)
            {
            case 0:
              s.set_fs_base(base, current() == _this());
              break;

            case 1:
              s.set_gs_base(base, current() == _this());
              break;

            default: return Kobject_iface::commit_result(-L4_err::EInval);
            }
          return Kobject_iface::commit_result(0);
        }

      case Thread::Op_segment_info_amd64:
        out->values[0] = Gdt::gdt_data_user   | Gdt::Selector_user; // user_ds32
        out->values[1] = Gdt::gdt_code_user   | Gdt::Selector_user; // user_cs64
        out->values[2] = Gdt::gdt_code_user32 | Gdt::Selector_user; // user_cs32
        return Kobject_iface::commit_result(0, 3);

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

    if (EXPECT_FALSE(rcv->exception_triggered()))
      {
        // triggered exception pending
        Mem::memcpy_mwords(ts, &src->s, Ts::Reg_words);
        Continuation::User_return_frame const *urfp
          = reinterpret_cast<Continuation::User_return_frame const *>
              ((char*)&src->s._ip);

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
        ts->cs(cs & ~0x80);
      }

    auto &rcs = rcv->_cpu_state_();
    rcs.fs_base = access_once(&src->fs_base);
    rcs.gs_base = access_once(&src->gs_base);

    rcs.ds = access_once(&src->ds);
    rcs.es = access_once(&src->es);
    rcs.fs = access_once(&src->fs);
    rcs.gs = access_once(&src->gs);

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
        auto &sns = snd->_cpu_state_();

        dst->ds = sns.ds;
        dst->es = sns.es;
        dst->fs = sns.fs;
        dst->gs = sns.gs;
        dst->fs_base = sns.fs_base;
        dst->gs_base = sns.gs_base;

        if (EXPECT_FALSE(snd->exception_triggered()))
          {
            Mem::memcpy_mwords(&dst->s, ts, Ts::Reg_words + Ts::Code_words);
            Continuation::User_return_frame *d
              = reinterpret_cast<Continuation::User_return_frame *>
              ((char*)&dst->s._ip);

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
