//----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

#include <entry.h>

PROTECTED inline NEEDS[Thread::sys_gdt_x86]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *out)
{
  switch (utcb->values[0] & Opcode_mask)
    {
    case Op_gdt_x86: return sys_gdt_x86(tag, utcb, out);
    case Op_set_segment_base_amd64:
      {
        if (tag.words() < 2)
          return commit_result(-L4_err::EMsgtooshort);

        Mword base = access_once(utcb->values + 1);
        if (!Cpu::is_canonical_address(base))
          return commit_result(-L4_err::EInval);

        switch (utcb->values[0] >> 16)
          {
          case 0:
            _cpu_state.set_fs_base(base, current() == this);
            break;

          case 1:
            _cpu_state.set_gs_base(base, current() == this);
            break;

          default: return commit_result(-L4_err::EInval);
          }
        return Kobject_iface::commit_result(0);
      }
    case Op_segment_info_amd64:
      out->values[0] = Gdt::gdt_data_user   | Gdt::Selector_user; // user_ds32
      out->values[1] = Gdt::gdt_code_user   | Gdt::Selector_user; // user_cs64
      out->values[2] = Gdt::gdt_code_user32 | Gdt::Selector_user; // user_cs32
      return Kobject_iface::commit_result(0, 3);
    default:
      return commit_result(-L4_err::ENosys);
    };
}

PROTECTED inline
int
Thread::do_trigger_exception(Entry_frame *r, void *ret_handler)
{
  if (!exception_triggered())
    {
      _exc_cont.activate(r, ret_handler);
      return 1;
    }
  // else ignore change of IP because triggered exception already pending
  return 0;
}

PUBLIC inline
void
Thread::restore_exc_state()
{
  _exc_cont.restore(regs());
}

PRIVATE static inline
Return_frame *
Thread::trap_state_to_rf(Trap_state *ts)
{
  char *im = reinterpret_cast<char*>(ts + 1);
  return reinterpret_cast<Return_frame*>(im)-1;
}

PROTECTED static inline NEEDS[Thread::trap_state_to_rf, Thread::sanitize_user_flags]
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  if (EXPECT_FALSE((tag.words() * sizeof(Mword)) < sizeof(Trex)))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
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
      urf.flags(sanitize_user_flags(urf.flags()));
      rcv->_exc_cont.set(trap_state_to_rf(ts), &urf);
    }
  else
    {
      Mem::memcpy_mwords(ts, &src->s, Ts::Words);
      // sanitize flags
      ts->flags(sanitize_user_flags(ts->flags()));
      // don't allow to overwrite the code selector!
      ts->cs(cs & ~0x80);
    }

  rcv->_cpu_state.fs_base = access_once(&src->fs_base);
  rcv->_cpu_state.gs_base = access_once(&src->gs_base);

  rcv->_cpu_state.ds = access_once(&src->ds);
  rcv->_cpu_state.es = access_once(&src->es);
  rcv->_cpu_state.fs = access_once(&src->fs);
  rcv->_cpu_state.gs = access_once(&src->gs);

  if (rcv == current())
    rcv->_cpu_state.gdt_user_entries.load();

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PROTECTED static inline NEEDS[Thread::trap_state_to_rf]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;
  Utcb *rcv_utcb = rcv->utcb().access();
  Trex *dst = reinterpret_cast<Trex *>(rcv_utcb->values);
    {
      auto guard = lock_guard(cpu_lock);

      dst->ds = snd->_cpu_state.ds;
      dst->es = snd->_cpu_state.es;
      dst->fs = snd->_cpu_state.fs;
      dst->gs = snd->_cpu_state.gs;
      dst->fs_base = snd->_cpu_state.fs_base;
      dst->gs_base = snd->_cpu_state.gs_base;

      if (EXPECT_FALSE(snd->exception_triggered()))
        {
          Mem::memcpy_mwords(&dst->s, ts, Ts::Reg_words + Ts::Code_words);
          Continuation::User_return_frame *d
            = reinterpret_cast<Continuation::User_return_frame *>
            ((char*)&dst->s._ip);

          snd->_exc_cont.get(d, trap_state_to_rf(ts));
        }
      else
        Mem::memcpy_mwords(&dst->s, ts, Ts::Words);

      if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
        snd->transfer_fpu(rcv);
    }
  return true;
}


