#pragma once

#include <cxx/atomic>

#include "ipc_sender.h"
#include "irq_chip.h"
#include "kobject_helper.h"
#include "member_offs.h"
#include "sender.h"
#include "context.h"
#include "config.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "entry_frame.h"

#if defined (CONFIG_JDB)
#include "string_buffer.h"
#endif // CONFIG_JDB

class Ram_quota;
class Thread;


/** Hardware interrupts.  This class encapsulates hardware IRQs.  Also,
    it provides a registry that ensures that only one receiver can sign up
    to receive interrupt IPC messages.
 */
class Irq : public Irq_base, public cxx::Dyn_castable<Irq, Kobject>
{
  MEMBER_OFFSET();
  typedef Slab_cache Allocator;
  Irq() = delete;

public:
  enum Op
  {
    Op_eoi_1      = 0, // Irq_sender + Irq_semaphore
    Op_compat_attach     = 1,
    Op_trigger    = 2, // Irq_sender + Irq_semaphore
    Op_compat_chain      = 3,
    Op_eoi_2      = 4, // Icu + Irq_sender + Irq_semaphore
    Op_compat_detach     = 5,
  };

  void *operator new (size_t, void *p) noexcept
  { return p; }

  void operator delete (void *_l) noexcept;

  template<typename T>
  static T* allocate(Ram_quota *q) noexcept
  {
    void *nq = allocator()->q_alloc(q);
    if (nq)
      return new (nq) T(q);

    return 0;
  }

  explicit __attribute__((nonnull))
  Irq(Ram_quota *q) : _q(q) {}

  void destroy(Kobject ***rl) override;

#if defined (CONFIG_JDB)
  virtual void dbg_print(String_buffer *buf, Kobject_common *link) const = 0;
#endif // CONFIG_JDB

protected:
  Ram_quota *_q;
  Context::Drq _drq;

  static Irq::Allocator *allocator() noexcept;


  static int get_irq_opcode(L4_msg_tag tag, Utcb const *utcb)
  {
    if (tag.proto() == L4_msg_tag::Label_irq && tag.words() == 0)
      return Op_trigger;
    if (EXPECT_FALSE(tag.words() < 1))
      return -1;

    return access_once(utcb->values) & 0xffff;
  }

  L4_msg_tag dispatch_irq_proto(Unsigned16 op, bool may_unmask)
  {
    switch (op)
      {
      case Op_eoi_1:
      case Op_eoi_2:
        if (may_unmask)
          unmask();
        return L4_msg_tag(L4_msg_tag::Schedule); // no reply

      case Op_trigger:
        log();
        hit(0);
        return L4_msg_tag(L4_msg_tag::Schedule); // no reply

      default:
        return commit_result(-L4_err::ENosys);
      }
  }


};


/**
 * IRQ Kobject to send IPC messages to a receiving thread.
 */
class Irq_sender
: public Kobject_h<Irq_sender, Irq>,
  public Ipc_sender<Irq_sender>
{
public:
  enum Op {
    Op_attach = 0,
    Op_detach = 1,
    Op_bind     = 0x10,
  };

  explicit Irq_sender(Ram_quota *q = 0) noexcept
  : Kobject_h<Irq_sender, Irq>(q), _queued(0), _irq_thread(0), _irq_id(~0UL)
  {
    hit_func = &hit_level_irq;
  }

  /**
   * Bind a receiver to this device interrupt.
   * \param t           the receiver that wants to receive IPC messages for this
   *                    IRQ
   * \param utcb        The input UTCB
   * \param utcb_out    The output UTCB
   *
   * \retval 0        on success, `t` is the new IRQ handler thread
   * \retval -EINVAL  if `t` is not a valid thread.
   * \retval -EBUSY   if another detach operation is in progress or object already
   *                  destroyed.
   *
   * \retval L4_error::Not_existent  Irq_sender object was deleted
   */
  L4_msg_tag bind_irq_thread(Thread *t, Utcb const *utcb, Utcb *utcb_out)
  {
    if (t == nullptr)
      return commit_result(-L4_err::EInval);

    Lock_guard<Lock> guard;
    if (!guard.check_and_lock(&existence_lock))
      return commit_error(utcb_out, L4_error::Not_existent);

    Thread *old = _irq_thread.load(cxx::memory_order_relaxed);
    for (;;)
      {

        if (old == t)
          break;

        if (EXPECT_FALSE(old == detach_in_progress()))
          return commit_result(-L4_err::EBusy);

        if (_irq_thread.compare_exchange_strong(old, t, cxx::memory_order_acquire))
          break;
      }

    // note: this is a possible race on user-land where the label of an IRQ might
    // become inconsistent with the attached thread. The user is responsible to
    // synchronize Irq::attach calls to prevent this.
    _irq_id = access_once(&utcb->values[1]);

    if (old == t)
      return commit_result(0);

    auto g = lock_guard(cpu_lock);
    bool reinject = false;

    if (is_valid_thread(old))
      {
        switch (old->Receiver::abort_send(this))
          {
          case Receiver::Abt_ipc_done:
            break; // was not queued

          case Receiver::Abt_ipc_cancel:
            reinject = true;
            break;

          default:
            // this must not happen as this is only the case
            // for IPC including message items and an IRQ never
            // sends message items.
            panic("IRQ IPC flagged as in progress");
          }

        if (old->dec_ref() == 0)
          delete old;
      }

    t->inc_ref();
    if (Cpu::online(t->home_cpu()))
      _chip->set_cpu(pin(), t->home_cpu());

    if (reinject)
      {
        // might have changed between the CAS and taking the lock
        t = _irq_thread.load();
        if (EXPECT_TRUE(is_valid_thread(t)))
          send(t);
      }

    return commit_result(0);
  }

  Receiver *owner() const
  { return _irq_thread.load(cxx::memory_order_relaxed); }

  void switch_mode(bool is_edge_triggered) override
  {
    hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
  }

  void destroy(Kobject ***rl) override
  {
    auto g = lock_guard(cpu_lock);
    Irq::destroy(rl);
    // Must be done _after_ returning from Irq::destroy() to make sure that the
    // existence lock was finally released by the last owner (the existence lock
    // was already invalidated before) -- see also Irq_sender::bind_irq_thread().
    (void)detach_irq_thread();
  }

  int queued() const
  {
    return _queued.load(cxx::memory_order_relaxed);
  }

  /**
   * Predicate used to figure out if the sender shall be enqueued
   * for sending a second message after sending the first.
   */
  bool requeue_sender()
  { return consume() > 0; }

  /**
   * Predicate used to figure out if the sender shall be dequeued after
   * sending the request.
   */
  bool dequeue_sender()
  { return consume() < 1; }

  Syscall_frame *transfer_msg(Receiver *recv)
  {
    Syscall_frame* dst_regs = recv->rcv_regs();

    // set ipc return value: OK
    dst_regs->tag(L4_msg_tag(0));

    // set the IRQ label
    dst_regs->from(_irq_id);

    return dst_regs;
  }

  void modify_label(Mword const *todo, int cnt) override;
  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                     Utcb const *utcb, Utcb *);

  Mword obj_id() const override
  { return _irq_id; }


#if defined (CONFIG_JDB)
  void dbg_print(String_buffer *buf, Kobject_common *link) const override
  {
    buf->printf(" L=%lx T=%lx Q=%d",
                obj_id(),
                link ?  link->dbg_info()->dbg_id() : 0,
                const_cast<Irq_sender *>(this)->queued());
  }
#endif // CONFIG_JDB

protected:
  static Thread *detach_in_progress()
  { return reinterpret_cast<Thread *>(1); }

  static bool is_valid_thread(Thread const *t)
  { return t > detach_in_progress(); }

  cxx::atomic<Smword> _queued;
  cxx::atomic<Thread *> _irq_thread;

private:
  Mword _irq_id;

  static Context::Drq::Result handle_remote_hit(Context::Drq *, Context *target, void *arg);
  void _hit_level_irq(Upstream_irq const *ui);
  static void hit_level_irq(Irq_base *i, Upstream_irq const *ui);
  void _hit_edge_irq(Upstream_irq const *ui);
  static void hit_edge_irq(Irq_base *i, Upstream_irq const *ui);
  L4_msg_tag sys_detach(L4_fpage::Rights rights);
  L4_msg_tag sys_bind(L4_msg_tag tag, L4_fpage::Rights rights, Utcb const *utcb,
                      Utcb *utcb_out);

  /**
   * Release an interrupt.
   *
   * \retval 0        on success.
   * \retval -ENOENT  if there was no receiver attached.
   * \retval -EBUSY   when there is another detach operation in progress.
   */
  int detach_irq_thread()
  {
    Mem::mp_release();
    Thread *t = _irq_thread.load(cxx::memory_order_relaxed);
    for (;;)
      {

        if (t == detach_in_progress())
          return -L4_err::EBusy;

        if (t == nullptr)
          return -L4_err::ENoent;

        if (EXPECT_TRUE(_irq_thread.compare_exchange_strong(t, detach_in_progress())))
          break;
      }

    auto guard = lock_guard(cpu_lock);
    mask();

    t->Receiver::abort_send(this);

    _irq_thread.store(nullptr, cxx::memory_order_release);
    // release cpu-lock early, actually before delete
    guard.reset();

    if (t->dec_ref() == 0)
      delete t;

    return 0;
  }

  /**
   * Consume all interrupts.
   * @return number of IRQs that are still pending -- this is always 0.
   */
  Smword consume()
  {
    Smword old = _queued.load(cxx::memory_order_relaxed);
    while (!_queued.compare_exchange_strong(old, 0L, cxx::memory_order_acquire))
      ;

    if (old >= 2 && hit_func == &hit_edge_irq)
      unmask();

    return 0L;
  }

  Smword queue()
  {
    return _queued.fetch_add(1);
  }

  void send(Thread *t)
  {
    if (EXPECT_FALSE(t->home_cpu() != current_cpu()))
      t->drq(&_drq, handle_remote_hit, this,
             Context::Drq::No_wait);
    else
      send_msg(t, true);
  }


};

#if 0
//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert_opt.h"
#include "config.h"
#include "cpu_lock.h"
#include "globals.h"
#include "ipc_sender.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "lock_guard.h"
#include "minmax.h"
#include "std_macros.h"
#include "thread_object.h"
#include "thread_state.h"
#include "l4_buf_iter.h"
#include "vkey.h"
#endif



