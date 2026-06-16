#pragma once

#include <l4_error.h>
#include <receiver.h>
#include <sender.h>
#include <context_base.h>
#include <map_util.h>
#include <logdefs.h>
#include <ipc_timeout.h>
#include <l4_types.h>
#include <tb_entry.h>
#include <l4_buf_iter.h>
#include <entry.h>
#include <system_clock.h>

#include <cassert>

class Thread;
class Syscall_frame;

typedef Context_ptr_base<Thread> Thread_ptr;

class Thread_ipc_base
{
public:
  Trap_state *utcb_handler_ts() const
  { return reinterpret_cast<Trap_state *>(_utcb_handler); }

protected:
  using Rcv_state = Receiver::Rcv_state;

  void *_utcb_handler;

  struct Check_sender
  {
    enum R
    {
      Ok = 0,
      Open_wait_flag = 0x1,
      Queued = 2,
      Done   = 4,
      Failed = 5,
    };

    static_assert((unsigned)Rcv_state::Open_wait_flag == (unsigned)Open_wait_flag,
                  "Rcv_state and Check_sender flags must be compatible");

    R s;

    constexpr Check_sender(Receiver::Rcv_state s)
    : s((R)(s.s & Open_wait_flag))
    {}

    constexpr Check_sender(R s) noexcept : s(s) {}
    Check_sender() = default;

    constexpr bool is_ok() const { return !(s & ~1u); }
    constexpr bool is_open_wait() const { return s & Open_wait_flag; }
  };

  struct Ipc_remote_request
  {
    L4_msg_tag tag;
    Thread *partner;
    bool zero_timeout;
    bool have_rcv;

    Check_sender result;
  };

  class Buf_utcb_saver
  {
  public:
    explicit Buf_utcb_saver(Utcb const *u)
    {
      buf_desc = u->buf_desc;
      buf[0] = u->buffers[0];
      buf[1] = u->buffers[1];
    }

    void restore(Utcb *u) const
    {
      u->buf_desc = buf_desc;
      u->buffers[0] = buf[0];
      u->buffers[1] = buf[1];
    }

  private:
    L4_buf_desc buf_desc;
    Mword buf[2];
  };

  /**
   * Save critical contents of UTCB during nested IPC.
   */
  class Pf_msg_utcb_saver : public Buf_utcb_saver
  {
  public:
    explicit Pf_msg_utcb_saver(Utcb const *u) : Buf_utcb_saver(u)
    {
      msg[0] = u->values[0];
      msg[1] = u->values[1];
    }

    void restore(Utcb *u) const
    {
      Buf_utcb_saver::restore(u);
      u->values[0] = msg[0];
      u->values[1] = msg[1];
    }

  private:
    Mword msg[2];
  };

  struct Log_pf_invalid : public Tb_entry
  {
    Mword pfa;
    Cap_index cap_idx;
    Mword err;
    void print(String_buffer *buf) const
    {
      buf->printf("InvCap C:%lx pfa=%lx err=%lx",
                  cxx::int_value<Cap_index>(cap_idx), pfa, err);
    }
  };

  struct Log_exc_invalid : public Tb_entry
  {
    Cap_index cap_idx;
    void print(String_buffer *buf) const
    {
      buf->printf("InvCap C:%lx", cxx::int_value<Cap_index>(cap_idx));
    }
  };
};

template<typename THREAD>
class Thread_ipc :
  public Sender,
  public Thread_ipc_base
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

  template<typename T>
  Thread *_thread(T *t)
  { return t; }

  template<typename T>
  Thread const *_thread(T const *t)
  { return t; }

  bool _xcpu_state_change(Mword mask, Mword add, bool lazy_q = false)
  {
    return _this()->xcpu_state_change(mask, add, lazy_q);
  }

  static void clear_fpu_before_receive(Thread *partner)
  {
    if (partner->_utcb_handler
        || partner->utcb().access()->inherit_fpu())
      partner->spill_fpu_if_owner();
  }

  static void set_partner_ready(Thread *partner)
  {
    partner->state.change_dirty(~Thread_ipc_mask, Thread_ready);
    if (partner->home_cpu() == current_cpu() && current() != partner)
      Sched_context::rq.current().ready_enqueue(partner->sched());
  }

  static bool vcpu_exception_upcall(Thread *t, Trap_state *ts)
  {
    Vcpu_state *vcpu = t->vcpu_state().access();

    if (!t->vcpu_exceptions_enabled(vcpu))
      return true;

    if (t->exception_triggered())
      return false;

    // before entering kernel mode to have original fpu state before
    // enabling fpu
    t->save_fpu_state_to_utcb(ts, t->utcb().access());

    t->spill_user_state();

    if (t->vcpu_enter_kernel_mode(vcpu))
      {
        // enter_kernel_mode has switched the address space from user to
        // kernel space, so reevaluate the address of the VCPU state area
        vcpu = t->vcpu_state().access();
      }

    LOG_TRACE("VCPU events", "vcpu", t, Vcpu_log,
        l->type = 2;
        l->state = vcpu->saved_state();
        l->ip = ts->ip();
        l->sp = ts->sp();
        l->trap = ts->trapno();
        l->err = ts->error();
        l->space = t->vcpu_user_space() ? static_cast<Task*>(t->vcpu_user_space())->dbg_id() : ~0;
        );
    vcpu->_regs.s = *ts;
    Entry::vcpu_return_to_kernel(t, vcpu->_entry_ip, vcpu->_sp, t->vcpu_state().usr().get());
    // does not return

    return false; // this is a dummy
 }

  static
  L4_error map_one_item(Thread *snd, L4_msg_item item, L4_fpage sfp,
                        Ref_ptr<Task> const &receiver_t,
                        L4_buf_iter::Item const *buf,
                        Utcb *rcv_utcb, Kobject::Reap_list *rl);

private:
  // Used by Thread::ipc_send_msg().
  Syscall_frame *_snd_regs;
  Mword _from_spec;
  // Used when the IPC receiver executes ipc_send_msg() in the context of the
  // next sender. Otherwise we can use `rights` from `do_ipc()` directly.
  L4_fpage::Rights _ipc_send_rights;

protected:
  // More ipc state
  Thread_ptr _pager{Thread_ptr::Invalid};
  Thread_ptr _exc_handler{Thread_ptr::Invalid};

public:
  void ipc_send_msg(Receiver *recv, bool open_wait) override;
  void modify_label(Mword const *todo, int cnt) override;
  static bool transfer_msg_items(L4_msg_tag const &tag, Thread* snd, Utcb *snd_utcb,
                                 Thread *rcv, Utcb *rcv_utcb,
                                 L4_fpage::Rights rights);

  void set_ipc_from_spec(Mword from_spec, bool do_set = true)
  {
    if (do_set)
      _from_spec = from_spec;
  }

  void do_ipc(L4_msg_tag const &tag, Thread *partner,
              bool have_receive, Sender *sender, L4_timeout_pair t,
              Syscall_frame *regs, L4_fpage::Rights rights);

  bool handle_page_fault_pager(Address pfa, Mword error_code,
                               L4_msg_tag::Protocol protocol);

  /* return 1 if exception could be handled
   * return 0 if not for send_exception and halt thread
   */
  int send_exception(Trap_state *ts)
  {
    assert(cpu_lock.test());

    if (!vcpu_exception_upcall(_this(), ts))
      return 1;

    L4_fpage::Rights rights = L4_fpage::Rights(0);
    Kobject_iface *handler = _exc_handler.ptr(_this()->space(), &rights);

    if (EXPECT_FALSE(!handler))
      {
        /* no exception handler (anymore), put thread to sleep */
        LOG_TRACE("Exception invalid handler", "ieh", _this(), Log_exc_invalid,
                  l->cap_idx = _exc_handler.raw());
        if (EXPECT_FALSE(_this()->space()->is_sigma0()))
          {
            ts->dump();
            panic("Sigma0 raised an exception");
          }

        handler = _this(); // block on ourselves
      }

    _this()->state.change(~Thread_cancel, Thread_in_exception);

    return exception(handler, ts, rights);
  }

private:

  bool exception(Kobject_iface *handler, Trap_state *ts, L4_fpage::Rights rights);

  static Context::Drq::Result
  handle_remote_ipc_send(Drq *src, Context *, void *_rq);

  Check_sender
  remote_handshake_receiver(L4_msg_tag const &tag, Thread *partner,
                            bool have_receive, L4_timeout snd_t);

  class Rcv_side_item
  {
  private:
    Mword *_i;

  public:
    constexpr explicit Rcv_side_item(Mword *item) : _i(item) {}

    constexpr void copy_snd_item(L4_msg_item item, L4_fpage sfp) const
    {
      _i[0] = (item.raw() & ~0x0ff6) | (sfp.raw() & 0x0ff0);
    }

    constexpr void local_fpage_received(L4_fpage sfp) const
    {
      _i[0] |= 6;
      _i[1] = sfp.raw();
    }

    constexpr void obj_id_received(Mword id_n_rights) const
    {
      _i[0] |= 4;
      _i[1] = id_n_rights;
    }

    constexpr void flag_empty_map() const
    {
      _i[0] |= 2;
    }
  };

  static bool
  try_transfer_local_id(L4_buf_iter::Item const *const buf,
                        L4_fpage sfp, Rcv_side_item rcv_item, Thread* snd,
                        Thread *rcv)
  {
    if (buf->b.is_rcv_id())
      {
        if (snd->space() == rcv->space())
          {
            rcv_item.local_fpage_received(sfp);
            return true;
          }
        else
          {
            Obj_space::Capability cap = snd->space()->lookup(sfp.obj_index());
            Kobject_iface *o = cap.obj();
            if (EXPECT_TRUE(o && o->is_local(rcv->space())))
              {
                Mword rights = cap.rights()
                               & cxx::int_value<L4_fpage::Rights>(sfp.rights());
                rcv_item.obj_id_received(o->obj_id() | rights);
                return true;
              }
          }
      }
    return false;
  }

  [[nodiscard]] static bool
  copy_utcb_to_utcb(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                    L4_fpage::Rights rights)
  {
    assert (cpu_lock.test());

    Utcb *snd_utcb = snd->utcb().access();
    Utcb *rcv_utcb = rcv->utcb().access();
    Mword s = tag.words();
    Mword r = Utcb::Max_words;

    Mem::memcpy_mwords(rcv_utcb->values, snd_utcb->values, r < s ? r : s);

    bool success = true;
    if (tag.items())
      success = transfer_msg_items(tag, snd, snd_utcb, rcv, rcv_utcb, rights);

    if (success
        && tag.transfer_fpu()
        && rcv_utcb->inherit_fpu()
        && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    return success;
  }

  [[nodiscard]] bool
  copy_utcb_to(L4_msg_tag tag, Thread* receiver,
                       L4_fpage::Rights rights)
  {
    // we cannot copy trap state to trap state!
    assert (!_this()->_utcb_handler || !receiver->_utcb_handler);
    if (EXPECT_FALSE(this->_utcb_handler != 0))
      return _this()->copy_ts_to_utcb(tag, _this(), receiver, rights);
    else if (EXPECT_FALSE(receiver->_utcb_handler != 0))
      return _this()->copy_utcb_to_ts(tag, _this(), receiver, rights);
    else
      return copy_utcb_to_utcb(tag, _this(), receiver, rights);
  }

  bool transfer_msg(L4_msg_tag tag, Thread *receiver,
                    L4_fpage::Rights rights, bool open_wait)
  {
    Syscall_frame* dst_regs = receiver->rcv_regs();

    bool success = copy_utcb_to(tag, receiver, rights);
    tag.set_error(!success);
    dst_regs->tag(tag);
    dst_regs->from(_from_spec);

    // setup the reply capability in case of a call
    if (success && open_wait && _this()->is_partner(receiver))
      receiver->set_caller(_this(), rights);

    return success;
  }

  Check_sender
  check_sender(Thread *sender, bool zero_timeout)
  {
    if (EXPECT_FALSE(_this()->is_invalid()))
      {
        sender->utcb().access()->error = L4_error::Not_existent;
        return Check_sender::Failed;
      }

    if (auto ok = _this()->sender_ok(sender))
      return ok;

    if (zero_timeout)
      {
        sender->utcb().access()->error = L4_error::Timeout;
        return Check_sender::Failed;
      }

    sender->sender_enqueue(_this()->sender_list(), sender->sched_context()->prio());
    _this()->vcpu_set_irq_pending();
    return Check_sender::Queued;
  }


  bool remote_ipc_send(Ipc_remote_request *rq)
  {

#if 0
    LOG_MSG_3VAL(this, "rsend", (Mword)src, 0, 0);
    printf("CPU[%u]: remote IPC send ...\n"
           "  partner=%p [%u]\n"
           "  sender =%p [%u]\n"
           "  timeout=%u\n",
           current_cpu(),
           rq->partner, rq->partner->cpu(),
           src, src->cpu(),
           rq->timeout);
#endif

    Check_sender r = _thread(rq->partner)->check_sender(_this(), rq->zero_timeout);
    switch (r.s)
      {
      case Check_sender::Failed:
        _xcpu_state_change(~Thread_ipc_mask, 0);
        rq->result = Check_sender::Failed;
        return false;
      case Check_sender::Queued:
        rq->result = Check_sender::Queued;
        return false;
      default:
        break;
      }

    if (rq->tag.transfer_fpu())
      clear_fpu_before_receive(rq->partner);

    // We may need to grab locks but this is forbidden in a DRQ handler. So
    // transfer the IPC in usual thread code at the sender side. However, this
    // induces an overhead of two extra IPIs.
    if (rq->tag.items())
      {
        //LOG_MSG_3VAL(rq->partner, "pull", dbg_id(), 0, 0);
        _xcpu_state_change(~Thread_send_wait, Thread_ready);
        _thread(rq->partner)->state.change_dirty(~(Thread_ipc_mask | Thread_ready), Thread_ipc_transfer);
        rq->result = r;
        return true;
      }
    bool success = transfer_msg(rq->tag, rq->partner,
                                _ipc_send_rights, r.is_open_wait());
    if (success && rq->have_rcv)
      _xcpu_state_change(~Thread_send_wait, Thread_receive_wait);
    else
      _xcpu_state_change(~Thread_ipc_mask, 0);

    rq->result = success ? Check_sender::Done : Check_sender::Failed;
    set_partner_ready(rq->partner);

    return true;
  }

  /**
   * @pre cpu_lock must be held
   */
  Check_sender
  handshake_receiver(Thread *partner, L4_timeout snd_t)
  {
    assert(cpu_lock.test());

    Check_sender r = partner->check_sender(_this(), snd_t.is_zero());
    switch (r.s)
      {
      case Check_sender::Failed:
        break;
      case Check_sender::Queued:
        _this()->state.add_dirty(Thread_send_wait);
        break;
      default: // Ok
        partner->state.change_dirty(~(Thread_ipc_mask | Thread_ready), Thread_ipc_transfer);
        break;
      }
    return r;
  }

  Sender *get_next_sender(Sender *sender)
  {
    if (!_this()->sender_list()->empty())
      {
        if (sender) // closed wait
          {
            if (EXPECT_TRUE(sender->in_sender_list())
                && EXPECT_TRUE(_this()->sender_list() == sender->wait_queue()))
              return sender;
            return 0;
          }
        else // open wait
          {
            Sender *next = Sender::cast(_this()->sender_list()->first());
            assert (next->in_sender_list());
            _this()->set_partner(next);
            return next;
          }
      }
    return 0;
  }

  bool activate_ipc_partner(Thread *partner, Cpu_number current_cpu,
                            bool do_switch)
  {
    if (partner->home_cpu() == current_cpu)
      {
        partner->state.change_dirty(~Thread_ipc_transfer, Thread_ready);
        if (do_switch)
          {
            _this()->schedule_if(_this()->switch_exec_locked(
                  partner, Context::Not_Helping) != Context::Switch::Ok);
            return true;
          }
        else
          return _this()->deblock_and_schedule(partner);
      }

    partner->xcpu_state_change(~Thread_ipc_transfer, Thread_ready);
    return false;
  }

  /**
   * Wait until unlocked by an IPC sender or by a pre-programmed IPC timeout.
   *
   * The thread blocks until it is unblocked again, i.e. its ready flag is set, by
   * an IPC sender or by a pre-programmed IPC timeout.
   *
   * \param sender  Communication partner to receive from (closed wait), use
   *                nullptr for an open wait.
   *
   * \pre IPC Timeout, if any, must already be set up.
   */
  void do_receive_wait(Sender *sender)
  {
    _this()->state.del_dirty(Thread_ready);

    if (sender == this)
      _this()->switch_sched(_this()->sched(), &Sched_context::rq.current());

    _this()->schedule();

    assert (_this()->state.has(Thread_ready));
  }


  void set_ipc_error(L4_error const &e, Thread *rcv)
  {
    _this()->utcb().access()->error = e;
    rcv->utcb().access()->error = L4_error(e, L4_error::Rcv);
  }

  /**
   * \pre Runs on the sender CPU
   * \retval true when the IPC was aborted
   * \retval false iff the IPC was already finished
   */
  bool abort_send(L4_error const &e, Thread *partner)
  {
    _this()->state.del_dirty(Thread_full_ipc_mask);
    Receiver::Abort_state abt = Receiver::Abt_ipc_done;

    if (partner->home_cpu() == current_cpu())
      {
        if (sender_dequeue(partner->sender_list()))
          {
            partner->vcpu_update_state();
            abt = Receiver::Abt_ipc_cancel;
          }
        else if (partner->in_ipc(this))
          abt = Receiver::Abt_ipc_in_progress;
      }
    else
      abt = partner->Receiver::abort_send(this);

    switch (abt)
      {
      default:
      case Receiver::Abt_ipc_done:
        return false;
      case Receiver::Abt_ipc_cancel:
        _this()->utcb().access()->error = e;
        return true;
      case Receiver::Abt_ipc_in_progress:
        _this()->state.add_dirty(Thread_ipc_transfer);
        while (_this()->state.has(Thread_ipc_transfer))
          {
            _this()->state.del_dirty(Thread_ready);
            _this()->schedule();
          }
        return false;
      }
  }

  /**
   * \pre Runs on the sender CPU
   * \retval true iff the IPC was finished during the wait
   * \retval false iff the IPC was aborted with some error
   */
  bool do_send_wait(Thread *partner, L4_timeout snd_t)
  {
    IPC_timeout timeout;

    if (EXPECT_FALSE(snd_t.is_finite()))
      {
        Unsigned64 system_clock = System_clock::clock();
        Unsigned64 tval = snd_t.microsecs(system_clock, _this()->utcb().access(true));
        // Zero timeout or timeout expired already -- give up
        if (tval == 0 || tval <= system_clock)
          return !abort_send(L4_error::Timeout, partner);

        _this()->set_timeout(&timeout, tval);
      }

    Mword ipc_state;

    while (((ipc_state = _this()->state() & (Thread_send_wait | Thread_ipc_abort_mask))) == Thread_send_wait)
      {
        _this()->state.del_dirty(Thread_ready);
        _this()->schedule();
      }

    _this()->reset_timeout();

    if (EXPECT_TRUE(!(ipc_state & Thread_send_wait)))
      return true;

    if (EXPECT_FALSE(ipc_state & Thread_transfer_failed))
      {
        _this()->state.del_dirty(Thread_full_ipc_mask);
        return false;
      }

    if (EXPECT_FALSE(ipc_state & Thread_cancel))
      return !abort_send(L4_error::Canceled, partner);

    if (EXPECT_FALSE(ipc_state & Thread_timeout))
      return !abort_send(L4_error::Timeout, partner);

    return true;
  }

};

/**
 * Receiver-ready callback. Receivers call this function in the context of a
 * waiting sender when they get ready to receive a message from that sender
 * (in this case a thread).
 */
template<typename THREAD>
void
Thread_ipc<THREAD>::ipc_send_msg(Receiver *recv, bool open_wait)
{
  Syscall_frame *regs = _snd_regs;

  if (EXPECT_FALSE(_this()->home_cpu() != recv->home_cpu()
        && regs->tag().transfer_fpu()))
    clear_fpu_before_receive(nonull_static_cast<Thread*>(recv));

  sender_dequeue(recv->sender_list());
  recv->vcpu_update_state();
  bool success = transfer_msg(regs->tag(), nonull_static_cast<Thread*>(recv),
                              _ipc_send_rights, open_wait);

  Mword state_del;
  Mword state_add;
  if (EXPECT_TRUE(success))
    {
      regs->tag(L4_msg_tag(regs->tag(), 0));
      state_del = Thread_ipc_mask | Thread_ipc_transfer;
      state_add = Thread_ready;
      if (_this()->Receiver::prepared())
        state_add |= Thread_receive_wait;
    }
  else
    {
      regs->tag(L4_msg_tag(regs->tag(), L4_msg_tag::Error));
      state_del = Thread_ipc_transfer; // handle Abt_ipc_in_progress in
                                       // Thread::abort_send()
      state_add = Thread_transfer_failed | Thread_ready;
    }
  if (_xcpu_state_change(~state_del, state_add, true))
    recv->switch_to_locked(_this());
}

template<typename T>
void
Thread_ipc<T>::modify_label(Mword const *todo, int cnt)
{
  Mword l = _from_spec;
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((l & test_mask) == test)
        {
          Mword const del_mask = todo[i+2];
          Mword const add_mask = todo[i+3];

          l = (l & ~del_mask) | add_mask;
          _from_spec = l;
          return;
        }
    }
}

template<typename THREAD>
inline L4_error
Thread_ipc<THREAD>::map_one_item(Thread *snd, L4_msg_item item, L4_fpage sfp,
                                 Ref_ptr<Task> const &receiver_t,
                                 L4_buf_iter::Item const *buf,
                                 Utcb *rcv_utcb, Kobject::Reap_list *rl)
{
  Kobject::Locked<Task> rcv_t;
  if (EXPECT_FALSE(buf->b.compound()))
    {
      unsigned cap_br = buf->b.cap_br_idx();
      if (cap_br >= Utcb::Max_buffers)
        return L4_error::Overflow;

      L4_obj_ref tc(rcv_utcb->buffers[cap_br]);
      if (EXPECT_FALSE(!tc.valid()))
        return L4_error::Overflow;

      auto task_ref = receiver_t->lookup_local(tc.cap(), L4_fpage::Rights::CS());
      rcv_t = Kobject::Locked<Task>(task_ref.as<Task>());
    }
  else
    rcv_t = Kobject::Locked<Task>(receiver_t);

  if (EXPECT_FALSE(!rcv_t))
    return L4_error::Overflow;

  // Take the existence_lock for synchronizing maps -- kind of
  // coarse-grained. We could go for ref counting and more
  // fine-grained locks internally...

  auto c_lock = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
  return fpage_map(snd->space(), sfp, rcv_t.get(), L4_fpage(buf->d), item, rl);
}

template<typename THREAD>
bool
Thread_ipc<THREAD>::transfer_msg_items(L4_msg_tag const &tag,
    Thread* snd, Utcb *snd_utcb,
    Thread *rcv, Utcb *rcv_utcb,
    L4_fpage::Rights rights)
{
  Ref_ptr<Task> receiver_t(nonull_static_cast<Task*>(rcv->space()));
  L4_buf_iter mem_buffer(rcv_utcb, rcv_utcb->buf_desc.mem());
  L4_buf_iter io_buffer(rcv_utcb, rcv_utcb->buf_desc.io());
  L4_buf_iter obj_buffer(rcv_utcb, rcv_utcb->buf_desc.obj());
  L4_snd_item_iter snd_item(snd_utcb, tag.words());
  int items = tag.items();
  Mword *rcv_word = rcv_utcb->values + tag.words();

  Kobject::Reap_list rl;

  while (items > 0 && snd_item.more())
    {
      if (EXPECT_FALSE(!snd_item.next()))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

      L4_snd_item_iter::Item const *const item = snd_item.get();

      if (item->b.is_void())
        { // XXX: not sure if void fpages are needed
          // skip send item and current rcv_buffer
          --items;
          *rcv_word = 0;
          rcv_word += 2;
          continue;
        }

      L4_buf_iter *buf_iter = 0;

      switch (item->b.type())
        {
        case L4_msg_item::Map:
          switch (L4_fpage(item->d).type())
            {
            case L4_fpage::Memory: buf_iter = &mem_buffer; break;
            case L4_fpage::Io:     buf_iter = &io_buffer; break;
            case L4_fpage::Obj:    buf_iter = &obj_buffer; break;
            default: break;
            }
          break;
        default:
          break;
        }

      if (EXPECT_FALSE(!buf_iter))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

      L4_buf_iter::Item const *const buf = buf_iter->get();

      if (EXPECT_FALSE(buf->b.is_void() || buf->b.type() != item->b.type()))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

        {
          assert (item->b.type() == L4_msg_item::Map);
          L4_fpage sfp(item->d);

          Rcv_side_item rcv_item(rcv_word);

          rcv_item.copy_snd_item(item->b, sfp);

          rcv_word += 2;

          // diminish when sending via restricted IPC gates
          if (sfp.type() == L4_fpage::Obj)
            sfp.mask_rights(rights | L4_fpage::Rights::CRW() | L4_fpage::Rights::CD());

          if (!try_transfer_local_id(buf, sfp, rcv_item, snd, rcv))
            {
              // we need to do a real mapping
              L4_error err = map_one_item(snd, item->b, sfp, receiver_t, buf, rcv_utcb, &rl);
              if (EXPECT_FALSE(!err.ok()))
                {
                  snd->set_ipc_error(err, rcv);
                  return false;
                }

              if (err.empty_map())
                rcv_item.flag_empty_map();
            }
        }

      --items;

      if (!item->b.compound())
        buf_iter->next();
    }

  if (EXPECT_FALSE(items))
    {
      snd->set_ipc_error(L4_error::Overflow, rcv);
      return false;
    }

  return true;
}

template<typename T>
Context::Drq::Result
Thread_ipc<T>::handle_remote_ipc_send(Drq *src, Context *, void *_rq)
{
  Ipc_remote_request *rq = reinterpret_cast<Ipc_remote_request*>(_rq);
  bool r = nonull_static_cast<Thread*>(src->context())->remote_ipc_send(rq);
  //LOG_MSG_3VAL(src, "rse<", current_cpu(), (Mword)src, r);
  return r ? Drq::need_resched() : Drq::done();
}

/**
 * \pre Runs on the sender CPU
 */
template<typename T>
Thread_ipc_base::Check_sender
Thread_ipc<T>::remote_handshake_receiver(L4_msg_tag const &tag, Thread *partner,
                                         bool have_receive, L4_timeout snd_t)
{
  Ipc_remote_request rq;
  rq.tag = tag;
  rq.have_rcv = have_receive;
  rq.partner = partner;
  rq.zero_timeout = snd_t.is_zero();

  _this()->state.add_dirty(Thread_send_wait);

  if (tag.transfer_fpu())
    _this()->spill_fpu_if_owner();

  partner->drq(handle_remote_ipc_send, &rq);

  return rq.result;
}


/**
 * Send an IPC message and/or receive an IPC message.
 *
 * \param tag           message tag; specifies details about the send phase
 * \param from_spec     label for the receiver
 * \param partner       communication partner in the send phase; nullptr if
 *                      there is no send phase
 * \param have_receive  enable/disable receive phase
 * \param sender        communication partner in the receive phase; use
 *                      `nullptr` to accept any communication partner (open
 *                      wait)
 * \param t             timeouts for send phase and receive phase
 * \param regs          IPC registers of the initiator of this IPC
 * \param rights        object permissions; usually the permissions of the
 *                      invoked capability during a syscall
 *
 * \pre `cpu_lock` must be held
 * \pre may only be called on current_thread()
 *
 * This function blocks until the message can be sent/received, the respective
 * timeout hits, the IPC is canceled, or the thread is killed.
 *
 * \todo review closed wait handling of sender during possible
 *       quiescent states and blocking.
 */
template<typename T>
void
Thread_ipc<T>::do_ipc(L4_msg_tag const &tag, Thread *partner,
                      bool have_receive, Sender *sender, L4_timeout_pair t,
                      Syscall_frame *regs, L4_fpage::Rights rights)
{
  assert (cpu_lock.test());
  assert (_this() == current());

  bool do_switch = false;

  assert (!_this()->state.has(Thread_ipc_mask));

  _this()->prepare_receive(sender, have_receive ? regs : 0);
  bool activate_partner = false;
  Cpu_number current_cpu = ::current_cpu();

  if (partner)
    {
      _this()->reset_caller(partner);

      assert(!in_sender_list());
      do_switch = tag.do_switch();

      bool ok;
      Check_sender result;

      _ipc_send_rights = rights;

      if (EXPECT_TRUE(current_cpu == partner->home_cpu()))
        result = handshake_receiver(partner, t.snd);
      else
        {
          // We have either per se X-CPU IPC or we ran into an IPC during
          // migration (indicated by the pending DRQ).
          // This flag also prevents the receive path from accessing the thread
          // state of a remote sender.
          do_switch = false;
          _snd_regs = regs;
          result = remote_handshake_receiver(tag, partner, have_receive, t.snd);

          // this may block, so we could have been migrated here
          current_cpu = ::current_cpu();
        }

      switch (result.s)
        {
        case Check_sender::Done:
          ok = true;
          break;

        case Check_sender::Queued:
          // set _snd_regs to enable active receiving
          _snd_regs = regs;
          ok = _this()->do_send_wait(partner, t.snd); // --- blocking point ---
          current_cpu = ::current_cpu();
          break;

        case Check_sender::Failed:
          _this()->state.del_dirty(Thread_ipc_mask);
          ok = false;
          break;

        default:
          // ping pong with timeouts will profit from resetting the receiver´s
          // timeout, because it will require much less sorting overhead. If we
          // don't reset the timeout, the probability is very high that the
          // receiver timeout is in the timeout queue.
          if (EXPECT_TRUE(current_cpu == partner->home_cpu()))
            partner->reset_timeout();

          ok = transfer_msg(tag, partner, rights, result.is_open_wait());

          // transfer is also a possible migration point
          current_cpu = ::current_cpu();

          // switch to receiving state
          _this()->state.del_dirty(Thread_ipc_mask);
          if (ok && have_receive)
            _this()->state.add_dirty(Thread_receive_wait);

          activate_partner = partner != this;
          break;
        }

      if (EXPECT_FALSE(!ok))
        {
          // Send failed. Skip the receive phase (Thread_receive_wait was not
          // set) but still activate the partner (may include a switch to it)
          // to inform the partner about the failed IPC.
          regs->tag(L4_msg_tag(0, 0, L4_msg_tag::Error, 0));
        }
    }
  else
    {
      assert (have_receive);
      _this()->state.add_dirty(Thread_receive_wait);
    }

  {
    IPC_timeout rcv_timeout;
    // Indicates whether the receive timeout had already expired (is in the past
    // or is zero) when attempting to set up the timer for it.
    bool rcv_timeout_expired = false;

    // Holds the next sender if the IPC has a receive phase.
    Sender *next = 0;

    // A: If the send phase failed, it did not set the Thread_receive_wait flag
    // and the receive phase is skipped.
    // B: If we completed the send phase of a cross-core IPC directly on the
    // remote CPU (Check_sender::Done), we set the Thread_receive_wait flag with
    // xcpu_state_change. Between the execution of that state change and our
    // return from remote_handshake_receiver, a potential sender can now start
    // an IPC transfer to us, i.e. it makes us enter the receive phase
    // (Thread_ipc_transfer) or even directly completes the receive phase,
    // whereby in both cases also the Thread_receive_wait flag is cleared again.
    have_receive = _this()->state.has(Thread_receive_wait);

    if (have_receive)
      {
        assert (!in_sender_list());
        assert (!_this()->state.has(Thread_send_wait));
        next = get_next_sender(sender);

        if (!next)
          // If there is no next sender yet, we have to set up the receive
          // timeout, either for a direct switch to the IPC partner or for a
          // do_receive_wait.
          rcv_timeout_expired = !_this()->setup_timer(t.rcv, _this()->utcb().access(true), &rcv_timeout);
      }

    if (activate_partner)
      {
        // Directly switch to the IPC partner (receiver) only if all of the
        // following apply:
        //  - The Schedule flag is not set in the message tag, i.e. the sender
        //    is willing to donate its remaining time-slice to the receiver
        //    (provided by `do_switch`).
        //  - The home CPU of the receiver is the current CPU (provided by
        //    `do_switch`).
        //  - If the IPC has a receive phase, there must be no next sender
        //    already pending (next != nullptr implicates a receive phase) and
        //    the receive timeout must not have expired yet.
        //  - The IPC transfer qualifies for time-slice donation, i.e. the
        //    sender transitions into a closed wait (call) or the sender runs
        //    on a foreign scheduling context.
        bool do_direct_switch = do_switch && !next && !rcv_timeout_expired
          && (   (have_receive && sender) // closed wait (call)
              || (Sched_context::rq.current().current_sched() != _this()->sched()));

        if (activate_ipc_partner(partner, current_cpu, do_direct_switch))
          {
            // blocked so might have a new sender queued
            have_receive = _this()->state.has(Thread_receive_wait);
            if (have_receive && !next)
              next = get_next_sender(sender);
          }
      }

    if (next)
      {
        // Receive timeout might already been hit here if the next sender was
        // queued after we activated the IPC partner. In that case ignore the
        // timeout (clear the timeout flag) and transfer the message from the
        // pending sender anyway.
        _this()->state.change_dirty(~(Thread_ipc_mask | Thread_timeout), Thread_receive_in_progress);
        next->ipc_send_msg(_this(), !sender);
        _this()->state.del_dirty(Thread_ipc_mask);
      }
    else if (have_receive)
      {
        // If the receive timeout has not yet been hit and the IPC has not been
        // cancelled, enter receive wait.
        if ((_this()->state.dirty() & Thread_full_ipc_mask) == Thread_receive_wait)
          do_receive_wait(sender);
      }
  }

  if (sender && sender == partner)
    partner->reset_caller(_this());

  Mword state = _this()->state.dirty();

  if (EXPECT_TRUE (!(state & Thread_full_ipc_mask)))
    return;

  while (EXPECT_FALSE(state & Thread_ipc_transfer))
    {
      _this()->state.del_dirty(Thread_ready);
      _this()->schedule();
      state = _this()->state();
   }

  if (EXPECT_TRUE (!(state & Thread_full_ipc_mask)))
    return;

  if (state & Thread_ipc_mask)
    {
      Utcb *utcb = _this()->utcb().access(true);
      // the IPC has not been finished.  could be timeout or cancel
      // XXX should only modify the error-code part of the status code

      if (EXPECT_FALSE(state & Thread_cancel))
        {
          // we've presumably been reset!
          regs->tag(Kobject::commit_error(utcb, L4_error::R_canceled, regs->tag()));
        }
      else
        regs->tag(Kobject::commit_error(utcb, L4_error::R_timeout, regs->tag()));
    }
  _this()->state.del(Thread_full_ipc_mask);
}

/** Page fault handler.
    This handler suspends any ongoing IPC, then sets up page-fault IPC.
    Finally, the ongoing IPC's state (if any) is restored.
    @param pfa page-fault virtual address
    @param error_code page-fault error code.
 */
template<typename T> bool
Thread_ipc<T>::handle_page_fault_pager(Address pfa, Mword error_code,
                                       L4_msg_tag::Protocol protocol)
{
  auto guard = lock_guard(cpu_lock);

  L4_fpage::Rights rights;
  Kobject_iface *pager = _pager.ptr(_this()->space(), &rights);

  if (!pager)
    {
      WARN("CPU%u: Pager of %lx is invalid (pfa=" L4_PTR_FMT
           ", errorcode=" L4_PTR_FMT ") to %lx (pc=%lx)\n",
           cxx::int_value<Cpu_number>(current_cpu()), _this()->dbg_id(), pfa,
           error_code, cxx::int_value<Cap_index>(_pager.raw()), _this()->regs()->ip());


      LOG_TRACE("Page fault invalid pager", "ipfh", _this(), Log_pf_invalid,
                l->cap_idx = _pager.raw();
                l->err     = error_code;
                l->pfa     = pfa);

      _this()->kill();
      return true;
    }

  // set up a register block used as an IPC parameter block for the
  // page fault IPC
  Syscall_frame r;

  // save the UTCB fields affected by PF IPC
  Mword vcpu_irqs = _this()->vcpu_disable_irqs();
  Mem::barrier();
  Utcb *utcb = _this()->utcb().access(true);
  Pf_msg_utcb_saver saved_utcb_fields(utcb);


  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_msg_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  utcb->values[0] = PF::addr_to_msgword0(pfa, error_code);
  utcb->values[1] = _this()->regs()->ip(); //PF::pc_to_msgword1 (regs()->ip(), error_code));

  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  L4_msg_tag tag(2, 0, 0, protocol);

  r.timeout(timeout);
  r.tag(tag);
  r.from(0);
  r.ref(L4_obj_ref(_pager.raw(), L4_obj_ref::Ipc_call_ipc));
  pager->invoke(r.ref(), rights, &r, utcb);


  bool success = true;

  if (EXPECT_FALSE(r.tag().has_error()))
    {
      if (utcb->error.snd_phase()
          && (utcb->error.error() == L4_error::Not_existent)
          && PF::is_usermode_error(error_code)
          && !_this()->state.has(Thread_cancel))
        {
          success = false;
        }
    }
  else // no error
    {
      // If the pager rejects the mapping, it replies -1 in msg.w0
      if (EXPECT_FALSE (r.tag().proto() < 0 || utcb->values[0] == Mword(-1)))
        success = false;
    }

  // restore previous IPC state

  saved_utcb_fields.restore(utcb);
  Mem::barrier();
  _this()->vcpu_restore_irqs(vcpu_irqs);
  return success;
}

/**
 * \pre must run with local IRQs disabled (CPU lock held)
 * to ensure that handler does not disappear meanwhile.
 */
template<typename T>
bool
Thread_ipc<T>::exception(Kobject_iface *handler, Trap_state *ts, L4_fpage::Rights rights)
{
  Syscall_frame r;
  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  CNT_EXC_IPC;

  Mword vcpu_irqs = _this()->vcpu_disable_irqs();
  Mem::barrier();

  void *old_utcb_handler = _utcb_handler;
  _utcb_handler = ts;

  // fill registers for IPC
  Utcb *utcb = _this()->utcb().access(true);
  Buf_utcb_saver saved_state(utcb);

  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_msg_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  // clear regs
  L4_msg_tag tag(L4_exception_ipc::Msg_size, 0, L4_msg_tag::Transfer_fpu,
                 L4_msg_tag::Label_exception);

  r.tag(tag);
  r.timeout(timeout);
  r.from(0);
  r.ref(L4_obj_ref(_exc_handler.raw(), L4_obj_ref::Ipc_call_ipc));
  _this()->spill_user_state();
  handler->invoke(r.ref(), rights, &r, utcb);
  _this()->fill_user_state();

  saved_state.restore(utcb);

  _this()->state.del(Thread_in_exception);

  // restore original utcb_handler
  _utcb_handler = old_utcb_handler;
  Mem::barrier();
  _this()->vcpu_restore_irqs(vcpu_irqs);

  // FIXME: handle not existing exception handler properly
  // for now, just ignore any errors
  return 1;
}


