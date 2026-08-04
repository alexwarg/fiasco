#include "irq.h"
#include "wait_queue.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"

JDB_DEFINE_TYPENAME(Irq_sender, "\033[37mIRQ ipc\033[m");

static Kmem_slab _irq_allocator(sizeof (Irq_sender),
                                __alignof__ (Irq), "Irq");


namespace {
static Irq_base *irq_base_dcast(Kobject_iface *o)
{ return cxx::dyn_cast<Irq*>(o); }

struct Irq_base_cast
{
  Irq_base_cast()
  { Irq_base::dcast = &irq_base_dcast; }
};

static Irq_base_cast register_irq_base_cast;
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
L4_msg_tag
Irq_sender::bind_irq(Send_endpoint *tgt, Utcb const *utcb, Utcb *)
{
  Send_endpoint *old = _irq_target.load(cxx::memory_order_relaxed);
  for (;;)
    {
      if (old == tgt)
        break;

      if (EXPECT_FALSE(old == detach_in_progress()))
        return commit_result(-L4_err::EBusy);

      if (_irq_target.compare_exchange_strong(old, tgt, cxx::memory_order_acquire))
        break;
    }

  _irq_id = access_once(&utcb->values[1]);

  if (old == tgt)
    return commit_result(0);

  tgt->inc_ref();

  auto *t = cxx::dyn_cast<Thread *>(tgt);
  bool edge = is_edge_triggered();

  if (t)
    {
      hit_func = edge ? &hit_edge_irq : &hit_level_irq;
      if (Cpu::online(t->home_cpu()))
        _chip->set_cpu(pin(), t->home_cpu());
    }
  else
    hit_func = edge ? &hit_edge_irq_wq : &hit_level_irq_wq;

  if (!is_valid_target(old))
    return commit_result(0);

  if (sender_dequeue(old->rcv_queue()))
    {
      if (t)
        send(t);
      else
        send_to_wq(static_cast<Wait_queue *>(tgt));
    }

  old->release();
  return commit_result(0);
}

void
Irq_sender::modify_label(Mword const *todo, int cnt)
{
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((_irq_id & test_mask) == test)
	{
	  Mword const set_mask = todo[i+2];
	  Mword const set      = todo[i+3];

	  _irq_id = (_irq_id & ~set_mask) | set;
	  return;
	}
    }
}

inline void
Irq_sender::_hit_level_irq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  mask_and_ack();
  Upstream_irq::ack(ui);

  auto *t = target<Thread>();
  if (EXPECT_FALSE(!is_valid_target(t)))
    return;

  if (queue() == 0)
    send(t);
}

void
Irq_sender::hit_level_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_level_irq(ui); }

inline void
Irq_sender::_hit_edge_irq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());

  auto *t = target<Thread>();
  if (EXPECT_FALSE(!is_valid_target(t)))
    {
      mask_and_ack();
      Upstream_irq::ack(ui);
      return;
    }

  Smword q = queue();

  if (!q)
    ack();
  else
    mask_and_ack();

  Upstream_irq::ack(ui);
  if (q == 0)
    send(t);
}

void
Irq_sender::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_edge_irq(ui); }

inline void
Irq_sender::_hit_level_irq_wq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  mask_and_ack();
  Upstream_irq::ack(ui);

  auto *wq = target<Wait_queue>();
  if (EXPECT_FALSE(!is_valid_target(wq)))
    return;

  if (queue() == 0)
    send_to_wq(wq);
}

void
Irq_sender::hit_level_irq_wq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_level_irq_wq(ui); }

inline void
Irq_sender::_hit_edge_irq_wq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());

  auto *wq = target<Wait_queue>();
  if (EXPECT_FALSE(!is_valid_target(wq)))
    {
      mask_and_ack();
      Upstream_irq::ack(ui);
      return;
    }

  Smword q = queue();

  if (!q)
    ack();
  else
    mask_and_ack();

  Upstream_irq::ack(ui);
  if (q == 0)
    send_to_wq(wq);
}

void
Irq_sender::hit_edge_irq_wq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_edge_irq_wq(ui); }

void
Irq_sender::send_to_wq(Wait_queue *wq)
{
  if (Thread *receiver = wq->receive_msg_from(this, 255))
    {
      receiver->set_partner(this);
      send_msg(receiver, receiver->home_cpu() == current_cpu());
    }
}

L4_msg_tag
Irq_sender::sys_bind(L4_msg_tag tag, L4_fpage::Rights rights, Utcb const *utcb,
                     Utcb *utcb_out)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  auto *ep = Ko::first_cap(&tag, utcb, L4_fpage::Rights::CS())
    .deref<Send_endpoint>(&tag);
  if (!ep)
    return tag;

  return bind_irq(ep, utcb, utcb_out);
}

L4_msg_tag
Irq_sender::sys_detach(L4_fpage::Rights rights)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  auto res = detach_irq_thread();
  _irq_id = ~0UL;
  return commit_result(res);
}

L4_msg_tag
Irq_sender::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                    Utcb const *utcb, Utcb *utcb_out)
{
  L4_msg_tag tag = f->tag();
  int op = get_irq_opcode(tag, utcb);

  if (EXPECT_FALSE(op < 0))
    return commit_result(-L4_err::EInval);

  switch (tag.proto())
    {
    case L4_msg_tag::Label_kobject:
      switch (op)
        {
        case Op_bind: // the Rcv_endpoint opcode (equal to Ipc_gate::bind_thread)
          return sys_bind(tag, rights, utcb, utcb_out);
        default:
          return commit_result(-L4_err::ENosys);
        }

    case L4_msg_tag::Label_irq:
      return dispatch_irq_proto(op, _queued.load(cxx::memory_order_relaxed) < 1);

    case L4_msg_tag::Label_irq_sender:
      switch (op)
        {
        case Op_detach:
          return sys_detach(rights);

        default:
          return commit_result(-L4_err::ENosys);
        }
    default:
      return commit_result(-L4_err::EBadproto);
    }
}

Irq::Allocator *
Irq::allocator() noexcept
{ return &_irq_allocator; }

void
Irq::operator delete (void *_l) noexcept
{
  Irq *l = cxx::launder(static_cast<Irq*>(_l));
  assert(l->_q);
  allocator()->q_free(l->_q, l);
}

void
Irq::destroy(Kobject ***rl)
{
  // Irq_base::destroy() does unbind(). Therefore call Kobject::destroy() which
  // waits until the existence lock was finally released by the last owner (the
  // existence lock was already invalidated before). Otherwise this IRQ object
  // could be immediately bound to another IRQ chip by the (current) owner of
  // the existence lock of this IRQ object.
  Kobject::destroy(rl);
  Irq_base::destroy();
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
irq_sender_factory(Ram_quota *q, Space *,
                   L4_msg_tag, Utcb const *,
                   int *err)
{
  *err = L4_err::ENomem;
  return Irq::allocate<Irq_sender>(q);
}

static inline void __attribute__((constructor)) FIASCO_INIT_SFX(irq_sender_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_irq_sender, irq_sender_factory);
}
}

