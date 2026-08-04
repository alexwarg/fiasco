#pragma once

#include <cxx/atomic>

#include "ipc_sender.h"
#include "irq_chip.h"
#include "send_endpoint.h"
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
class Wait_queue;


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

  explicit Irq_sender(Ram_quota *q = nullptr) noexcept
  : Kobject_h<Irq_sender, Irq>(q), _queued(0), _irq_target(nullptr), _irq_id(~0UL)
  {
    hit_func = &hit_level_irq;
  }

  L4_msg_tag bind_irq(Send_endpoint *tgt, Utcb const *utcb, Utcb *);

  Send_endpoint *owner() const
  { return const_cast<Send_endpoint *>(_irq_target.load(cxx::memory_order_relaxed)); }

  void switch_mode(bool edge) override
  {
    bool wq_bound = (hit_func == &hit_level_irq_wq || hit_func == &hit_edge_irq_wq);
    if (wq_bound)
      hit_func = edge ? &hit_edge_irq_wq : &hit_level_irq_wq;
    else
      hit_func = edge ? &hit_edge_irq : &hit_level_irq;
  }

  void destroy(Kobject ***rl) override
  {
    auto g = lock_guard(cpu_lock);
    Irq::destroy(rl);
    // Must be done _after_ returning from Irq::destroy() to make sure that the
    // existence lock was finally released by the last owner (the existence lock
    // was already invalidated before) -- see also Irq_sender::bind_irq.
    if (auto *ep = _irq_target.load())
      if (is_valid_target(ep))
        ep->sender_deleted(_irq_id);
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

  Syscall_frame *transfer_msg(Context *receiver)
  {
    Syscall_frame* dst_regs = receiver->rcv_regs();

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
  static Send_endpoint *detach_in_progress()
  { return reinterpret_cast<Send_endpoint *>(1); }

  static bool is_valid_target(Send_endpoint const *t)
  { return t > detach_in_progress(); }

  template<typename T>
  T *target() const
  { return static_cast<T *>(_irq_target.load(cxx::memory_order_acquire)); }

  cxx::atomic<Smword> _queued;
  cxx::atomic<Send_endpoint *> _irq_target;

private:
  Mword _irq_id;

  void _hit_level_irq(Upstream_irq const *ui);
  static void hit_level_irq(Irq_base *i, Upstream_irq const *ui);
  void _hit_edge_irq(Upstream_irq const *ui);
  static void hit_edge_irq(Irq_base *i, Upstream_irq const *ui);
  void _hit_level_irq_wq(Upstream_irq const *ui);
  static void hit_level_irq_wq(Irq_base *i, Upstream_irq const *ui);
  void _hit_edge_irq_wq(Upstream_irq const *ui);
  static void hit_edge_irq_wq(Irq_base *i, Upstream_irq const *ui);
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
    Send_endpoint *old = _irq_target.load(cxx::memory_order_relaxed);
    for (;;)
      {
        if (old == detach_in_progress())
          return -L4_err::EBusy;

        if (old == nullptr)
          return -L4_err::ENoent;

        if (EXPECT_TRUE(_irq_target.compare_exchange_strong(old, detach_in_progress())))
          break;
      }

    auto guard = lock_guard(cpu_lock);
    mask();
    sender_dequeue(old->rcv_queue());
    _irq_target.store(nullptr, cxx::memory_order_release);
    guard.reset();
    old->release();

    return 0;
  }

  /**
   * Consume all interrupts.
   * @return number of IRQs that are still pending -- this is always 0.
   */
  Smword consume()
  {
    Smword old = _queued.load(cxx::memory_order_acquire);
    while (!_queued.compare_exchange_strong(old, 0L, cxx::memory_order_acquire))
      ;

    if (old >= 2 && (hit_func == &hit_edge_irq || hit_func == &hit_edge_irq_wq))
      unmask();

    return 0L;
  }

  Smword queue()
  {
    return _queued.fetch_add(1);
  }

  void send(Thread *t)
  {
    send_msg(t, t->home_cpu() == current_cpu());
  }

  void send_to_wq(Wait_queue *wq);

  bool is_edge_triggered() const
  { return hit_func == &hit_edge_irq || hit_func == &hit_edge_irq_wq; }
};

