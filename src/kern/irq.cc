#include "irq.h"
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

Context::Drq::Result
Irq_sender::handle_remote_hit(Context::Drq *, Context *target, void *arg)
{
  Irq_sender *irq = reinterpret_cast<Irq_sender*>(arg);
  irq->set_cpu(current_cpu());
  auto t = irq->_irq_thread.load(cxx::memory_order_acquire);
  if (EXPECT_TRUE(t == target))
    {
#ifdef CONFIG_JDB
      ++irq->_xcpu;
#endif
      if (EXPECT_TRUE(irq->send_msg(t, false)))
        return Context::Drq::no_answer_resched();
    }
  else if (EXPECT_TRUE(is_valid_thread(t)))
    t->drq(&irq->_drq, handle_remote_hit, irq,
           Context::Drq::No_wait);

  return Context::Drq::no_answer();
}

inline void
Irq_sender::_hit_level_irq(Upstream_irq const *ui)
{
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());
  mask_and_ack();
  Upstream_irq::ack(ui);

  auto t = _irq_thread.load(cxx::memory_order_acquire);
  if (EXPECT_FALSE(!is_valid_thread(t)))
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
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());

  auto t = _irq_thread.load(cxx::memory_order_acquire);
  if (EXPECT_FALSE(!is_valid_thread(t)))
    {
      mask_and_ack();
      Upstream_irq::ack(ui);
      return;
    }

  Smword q = queue();

  // if we get a second edge triggered IRQ before the first is
  // handled we can mask the IRQ.  The consume function will
  // unmask the IRQ when the last IRQ is dequeued.
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



L4_msg_tag
Irq_sender::sys_bind(L4_msg_tag tag, L4_fpage::Rights rights, Utcb const *utcb,
                     Utcb *utcb_out)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  Thread *thread;

  Ko::Rights t_rights;
  thread = Ko::deref<Thread>(&tag, utcb, &t_rights);
  if (!thread)
    return tag;

  if (EXPECT_FALSE(!(t_rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag res = bind_irq_thread(thread, utcb, utcb_out);

  return res;
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
  Irq *l = reinterpret_cast<Irq*>(_l);
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

