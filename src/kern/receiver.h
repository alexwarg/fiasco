#pragma once

#include <cxx/atomic>

#include "l4_error.h"
#include "logdefs.h"
#include "member_offs.h"
#include "timeout.h"
#include "prio_list.h"
#include "sender.h"
#include "std_macros.h"
#include "thread_state.h"
#include <vcpu_log.h>

class Syscall_frame;
class Sender;


/** A receiver.  This is a role class, and real receiver's must inherit from 
    it.  The protected interface is intended for the receiver, and the public
    interface is intended for the sender.

    The only reason this class inherits from Context is to force a specific 
    layout for Thread.  Otherwise, Receiver could just embed or reference
    a Context.
 */
template<typename CONTEXT>
class Receiver
{
  friend class Jdb_tcb;
  friend class Jdb_thread;

  MEMBER_OFFSET();

  CONTEXT *_this() { return static_cast<CONTEXT *>(this); }
  CONTEXT const *_this() const { return static_cast<CONTEXT const *>(this); }

public:
  struct Rcv_state
  {
    enum S
    {
      Not_receiving = 0x00,
      Open_wait_flag = 0x01,
      Ipc_receive   = 0x02, // with closed wait
      Ipc_open_wait = 0x03, // IPC with open wait
      Irq_receive   = 0x05, // IRQ (alawys open wait)
    };

    S s;

    constexpr Rcv_state(S s) noexcept : s(s) {}
    Rcv_state() = default;
    Rcv_state(Rcv_state const &) = default;
    Rcv_state(Rcv_state &&) = default;
    Rcv_state &operator = (Rcv_state const &) = default;
    Rcv_state &operator = (Rcv_state &&) = default;
    ~Rcv_state() = default;

    constexpr explicit operator bool () const { return s; }
    constexpr bool is_open_wait() const { return s & Open_wait_flag; }
    constexpr bool is_irq() const { return s & 0x4; }
    constexpr bool is_ipc() const { return s & 0x2; }
  };

  /** Head of sender list.
      @return a reference to the receiver's list of senders
   */
  Iterable_prio_list *sender_list()
  {
    return &_sender_list;
  }

  /** Head of sender list.
      @return a reference to the receiver's list of senders
   */
  Iterable_prio_list const *sender_list() const
  {
    return &_sender_list;
  }

  Rcv_state sender_ok(const Sender* sender, bool on_receiver_core) const
  {
    unsigned state = _this()->state();
    unsigned ipc_state = state & Thread_ipc_mask;

    // If Thread_send_in_progress is still set, we're still in the send phase
    if (EXPECT_FALSE(ipc_state != Thread_receive_wait))
      {
        if (!on_receiver_core)
          return  Rcv_state::Not_receiving;

        if (EXPECT_TRUE(! (state & Thread_vcpu_enabled)))
          return Rcv_state::Not_receiving;

        if (EXPECT_FALSE(ipc_state & Thread_ipc_mask))
          return Rcv_state::Not_receiving;

        auto *vcpu = _this()->vcpu_state().access();
        if (EXPECT_FALSE(!vcpu->irqs_enabled()))
          return Rcv_state::Not_receiving;

        vcpu_async_ipc(sender, vcpu);
        return Rcv_state::Irq_receive;
      }

    // Check open wait; test if this sender is really the first in queue
    auto partner = sender_list()->current_poi();
    if (EXPECT_TRUE(!partner
                    && (sender_list()->empty()
                       || Sender::is_head_of(sender, sender_list()))))
      return Rcv_state::Ipc_open_wait;

    // Check closed wait; test if this sender is really who we specified
    if (EXPECT_TRUE(sender == partner))
      return Rcv_state::Ipc_receive;

    return Rcv_state::Not_receiving;
  }

  class Caller
  {
  private:
    friend class Receiver;
    constexpr explicit Caller(Mword v) : _v(v) {}
    Mword _v;

  public:
    constexpr bool valid() const
    {
      return _v != 0;
    }

    constexpr L4_fpage::Rights rights() const
    {
      return L4_fpage::Rights(_v & 0x03);
    }

    constexpr Receiver *receiver() const
    {
      return reinterpret_cast<Receiver *>(_v & ~0x03ul);
    }

    constexpr operator Receiver * () const
    {
      return receiver();
    }
  };

  Caller caller() const
  {
    return Caller(_caller.load(cxx::memory_order_relaxed));
  }

  void set_caller(Receiver *caller, L4_fpage::Rights rights)
  {
    Mword nv = Mword(caller) | (cxx::int_value<L4_fpage::Rights>(rights) & 0x3);
    _caller.store(nv);
  }

  /**
   * Reset the caller field to 0 iff the current value is `old_caller`.
   */
  void reset_caller(Receiver const *old_caller)
  {
    Mword ov = _caller.load(cxx::memory_order_relaxed);
    // avoid exclusive access (do test, test-and-set)
    if (reinterpret_cast<Mword>(old_caller) != (ov & ~3ul))
      return;

    _caller.compare_exchange_strong(ov, 0UL);
  }

  void reset_caller()
  {
    _caller.store(0);
  }

  /** Return a reference to receiver's IPC registers.
      Senders call this function to poke values into the receiver's register set.
      @pre state() & Thread_ipc_receiving_mask
      @return pointer to receiver's IPC registers.
   */
  Syscall_frame *rcv_regs() const
  {
    //assert (state () & Thread_ipc_receiving_mask);

    return _rcv_regs;
  }

  /**
   * Set the IPC partner (sender).
   *
   * \pre Must only be invoked in a context where it is safe to access the
   *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
   *      at the Receiver.
   *
   * \param partner IPC partner
   */
  void set_partner(Sender* partner) __attribute__((nonnull))
  {
    sender_list()->set_poi(partner);
  }

  void reset_partner()
  {
    sender_list()->reset_poi();
  }

  /**
   * Check if this Receiver is in an IPC receive operation with the given sender.
   *
   * \pre Must only be invoked in a context where it is safe to access the
   *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
   *      at the Receiver.
   */
  bool in_ipc(Sender *sender) const
  {
    return (_this()->state() & Thread_receive_in_progress) && is_partner(sender);
  }

  void vcpu_update_state()
  {
    if (EXPECT_TRUE(!(_this()->state() & Thread_vcpu_enabled)))
      return;

    if (sender_list()->empty())
      _this()->vcpu_state().access()->clear_irq_pending();
  }

  bool rcv_prepared() const
  {
    return _rcv_regs;
  }

  /**
   * Check if the given sender is stored as the IPC partner of this Receiver.
   *
   * The IPC partner field is not reset after a receive operation is finished, so
   * it might contain an old value from the last receive operation (see also
   * `Receiver::in_ipc()`).
   *
   * \pre Must only be invoked in a context where it is safe to access the
   *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
   *      at the Receiver.
   */
  bool is_partner(Sender *s) const
  {
    return sender_list()->current_poi() == s;
  }

protected:
  Receiver() = default;

  void set_rcv_regs(Syscall_frame* regs)
  {
    _rcv_regs = regs;
  }

  void prepare_receive(Sender *partner, Syscall_frame *regs)
  {
    set_rcv_regs(regs);  // message should be poked in here
    if (partner)
      set_partner(partner);
    else
      reset_partner();
  }

  bool try_vcpu_irq_receive(unsigned ipc_state)
  {
    if ((ipc_state & (Thread_ipc_mask | Thread_vcpu_enabled)) != Thread_vcpu_enabled)
      return false;

    if (sender_list()->empty())
      return false;

    auto *vcpu = _this()->vcpu_state().access();
    if (EXPECT_FALSE(!vcpu->irqs_enabled()))
      return false;

    Sender *s = Sender::cast(sender_list()->dequeue_first());
    if (!s)
      return false;

    if (!sender_list()->empty())
      _this()->vcpu_set_irq_pending();

    vcpu_async_ipc(s, vcpu);
    s->ipc_send_msg(_this(), false);
    return true;
  }

private:
  Syscall_frame *_rcv_regs; // registers used for receive
  cxx::atomic<Mword> _caller{0};
  Iterable_prio_list _sender_list;

  template<typename VCPU_STATE>
  void vcpu_async_ipc(Sender const *sender, VCPU_STATE *vcpu) const
  {
    CONTEXT *self = const_cast<CONTEXT*>(_this());

    if (this == current())
      self->spill_user_state();

    if (self->vcpu_enter_kernel_mode(vcpu))
      vcpu = _this()->vcpu_state().access();

    LOG_TRACE("VCPU events", "vcpu", _this(), Vcpu_log,
        l->type = 1;
        l->state = vcpu->saved_state();
        l->ip = Mword(sender);
        l->sp = _this()->regs()->sp();
        l->space = ~0; //vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
        );

    self->_rcv_regs = &vcpu->_ipc_regs;
    vcpu->_regs.set_ipc_upcall();
    self->set_partner(const_cast<Sender*>(sender));
    self->state.add_dirty(Thread_receive_wait);
    self->vcpu_save_state_and_upcall();
  }
};


