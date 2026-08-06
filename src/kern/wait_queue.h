#pragma once

#include "kobject_helper.h"
#include "send_endpoint.h"
#include "prio_list.h"
#include "ref_obj.h"
#include "kmem_slab.h"

class Wait_queue final :
  public cxx::Dyn_castable<Wait_queue, Kobject_h<Wait_queue, Send_endpoint>>,
  public Ref_cnt_obj
{
  Wait_queue() = delete;

public:
  explicit Wait_queue(Ram_quota *q, Space *space) : _space(space), _quota(q)
  {
    inc_ref();
  }

  Space *space() const { return _space; }

  Locked_prio_list *rcv_queue() override { return &_wait_q; }
  Space *home_space() const override { return _space; }
  void inc_ref() override { Ref_cnt_obj::inc_ref(); }
  void release() override { if (dec_ref() == 0) delete this; }

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  // Non-virtual send entry point for use by Ipc_gate_dispatch and invoke.
  // Only handles the send (or call) path — no control protocol, no reply-and-wait.
  void do_send_ipc(Thread *ct, L4_obj_ref self, Syscall_frame *f, Utcb *utcb);

  enum Op
  {
    Op_register_del_irq = 5, // must be thread protocol Op codes
    Op_modify_senders   = 6, // must be thread protocol Op codes
    Op_bump_generation  = 0x100, // must not collide with thread protocol Op codes
  };

  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                     Utcb const *in, Utcb *out);

private:
  L4_msg_tag sys_register_delete_irq(L4_msg_tag tag, Utcb const *in, Utcb *out);
  L4_msg_tag sys_modify_senders(L4_msg_tag tag, Utcb const *in, Utcb *out);
  L4_msg_tag sys_bump_generation(L4_msg_tag tag, Utcb const *in, Utcb *out);

public:

  void destroy(Kobject ***reap_list) override;

  void operator delete (void *ptr);

  Locked_prio_list &wait_q() { return _wait_q; }

  Thread *receive_msg_from(Sender *sender, unsigned short prio)
  {
    Thread *receiver = dequeue_receiver();
    if (!receiver)
      receiver = enqueue_sender(sender, prio);

    return receiver;
  }

private:
  friend class Jdb_kobject;

  // Single queue used for either blocked senders or blocked receivers.
  // The two sides are mutually exclusive: a new arrival always checks first
  // whether the other side is waiting and dispatches immediately if so.
  // _has_senders distinguishes which side is currently queued.
  inline Sender *dequeue_sender();
  inline Sender *enqueue_receiver(Thread *ct, unsigned short prio);
  inline void do_receive(Thread *ct, Syscall_frame *f, Utcb *utcb);
  Thread *dequeue_receiver()
  {
    auto g = lock_guard(_wait_q.qlock());
    if (_has_senders)
      return nullptr;
    return static_cast<Thread*>(Sender::cast(_wait_q.dequeue_first_dirty()));
  }

  Thread *enqueue_sender(Sender *ct, unsigned short prio)
  {
    auto g = lock_guard(_wait_q.qlock());
    if (!_has_senders && !_wait_q.empty())
      return static_cast<Thread*>(Sender::cast(_wait_q.dequeue_first_dirty()));

    _has_senders = true;
    _wait_q.insert_dirty(ct->qitem(), prio);
    return nullptr;
  }

  bool check_generation(L4_buf_desc bdr) const
  {
    return (_generation.load(cxx::memory_order_acquire) & L4_buf_desc::generation_count_bfm_t::Low_mask) == bdr.generation_count();
  }


  Locked_prio_list _wait_q;
  bool _has_senders = false;
  cxx::atomic<unsigned> _generation{0};
  Space *_space;
  Ram_quota *_quota;
};
