
#include "ipc_gate.h"
#include "globalconfig.h"
#include "ipc_timeout.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "static_init.h"
#include "timer.h"

JDB_DEFINE_TYPENAME(Ipc_gate_obj, "\033[35mGate\033[m");
static Kmem_slab_t<Ipc_gate_obj> _ipc_gate_allocator("Ipc_gate");


Kobject_iface *
Ipc_gate_ctl::downgrade(unsigned long attr)
{
  if (attr & L4_msg_item::C_obj_right_1)
    return static_cast<Ipc_gate*>(static_cast<Ipc_gate_obj*>(this));
  else
    return this;
}

inline
L4_msg_tag
Ipc_gate_ctl::bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                          Syscall_frame *f, Utcb const *in, Utcb *)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag tag = f->tag();

  if (tag.words() < 2)
    return commit_result(-L4_err::EMsgtooshort);

  L4_fpage::Rights t_rights(0);
  Thread *t = Ko::deref<Thread>(&tag, in, &t_rights);
  if (!t)
    return tag;

  if (!(t_rights & L4_fpage::Rights::CS()))
    return commit_result(-L4_err::EPerm);

  Ipc_gate_obj *g = static_cast<Ipc_gate_obj*>(this);
  g->_id.store(in->values[1]);
  t->inc_ref();

  Thread *old = g->_thread.load(cxx::memory_order_relaxed);
  while (!g->_thread.compare_exchange_strong(old, t))
    ;

  Kobject::Reap_list r;
  if (old)
    old->put_n_reap(r.list());

  if (EXPECT_FALSE(!r.empty()))
    {
      auto l = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
      r.del_1();
    }

  g->unblock_all();
  current()->rcu_wait();
  g->unblock_all();

  if (EXPECT_FALSE(!r.empty()))
    {
      auto l = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
      r.del_2();
    }

  return commit_result(0);
}

inline
L4_msg_tag
Ipc_gate_ctl::get_infos(L4_obj_ref, L4_fpage::Rights,
                        Syscall_frame *, Utcb const *, Utcb *out)
  {
    Ipc_gate_obj *g = static_cast<Ipc_gate_obj*>(this);
    out->values[0] = g->id();
    return commit_result(0, 1);
  }


void
Ipc_gate_obj::unblock_all()
{
  while (::Prio_list_elem *h = _wait_q.first())
    {
      auto g1 = lock_guard(cpu_lock);
      Thread *w;
        {
          auto g2 = lock_guard(_wait_q.lock());
          if (EXPECT_FALSE(h != _wait_q.first()))
            continue;

          w = static_cast<Thread*>(Sender::cast(h));
          w->sender_dequeue(&_wait_q);
        }
      w->activate();
    }
}

void
Ipc_gate_obj::initiate_deletion(Kobject ***r)
{
  if (Thread *t = _thread.load(cxx::memory_order_acquire))
    t->ipc_gate_deleted(id());

  Kobject::initiate_deletion(r);
}

void
Ipc_gate_obj::destroy(Kobject ***r)
{
  Kobject::destroy(r);
  Thread *tmp = _thread.load(cxx::memory_order_acquire);
  if (tmp)
    {
      _thread.store(0, cxx::memory_order_release);
      unblock_all();
      tmp->put_n_reap(r);
    }
}

Ipc_gate_obj::Self_alloc *
Ipc_gate_obj::allocator()
{ return _ipc_gate_allocator.slab(); }

Ipc_gate_obj *
Ipc_gate::create(Ram_quota *q, Thread *t, Mword id)
{
  Auto_quota<Ram_quota> quota(q, sizeof(Ipc_gate_obj));

  if (EXPECT_FALSE(!quota))
    return 0;

  void *nq = Ipc_gate_obj::allocator()->alloc();
  if (EXPECT_FALSE(!nq))
    return 0;

  quota.release();
  return new (nq) Ipc_gate_obj(q, t, id);
}

void Ipc_gate_obj::operator delete (void *_f) noexcept
{
  Ipc_gate_obj *f = (Ipc_gate_obj*)_f;
  Ram_quota *p = f->_quota;
  asm ("" : "=m"(*f));

  allocator()->free(f);
  if (p)
    p->free(sizeof(Ipc_gate_obj));
}


void
Ipc_gate_ctl::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb *utcb)
{
  if (f->tag().proto() == L4_msg_tag::Label_kobject
      && (f->ref().op() & L4_obj_ref::Ipc_send))
    Kobject_h<Ipc_gate_ctl, Kobject_iface>::invoke(self, rights, f, utcb);
  else
    static_cast<Ipc_gate_obj*>(this)->Ipc_gate::invoke(self, rights, f, utcb);
}

L4_msg_tag
Ipc_gate_ctl::kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                      Syscall_frame *f, Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  if (EXPECT_FALSE(tag.proto() != L4_msg_tag::Label_kobject))
    return commit_result(-L4_err::EBadproto);

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  switch (in->values[0])
    {
    case Op_bind:
      return bind_thread(self, rights, f, in, out);
    case Op_get_info:
      return get_infos(self, rights, f, in, out);
    default:
      return static_cast<Ipc_gate_obj*>(this)->kobject_invoke(self, rights, f, in, out);
    }
}

inline
L4_error
Ipc_gate::block(Thread *ct, L4_timeout const &to, Utcb *u)
{
  Unsigned64 t = 0;
  if (!to.is_never())
    {
      t = to.microsecs(Timer::system_clock(), u);
      if (!t)
        return L4_error::Timeout;
    }

    {
      auto g = lock_guard(_wait_q.lock());
      ct->set_wait_queue(&_wait_q);
      ct->sender_enqueue(&_wait_q, ct->sched_context()->prio());
    }
  ct->state.change_dirty(~Thread_ready, Thread_send_wait);

  IPC_timeout timeout;
  if (t)
    {
      timeout.set(t, current_cpu());
      ct->set_timeout(&timeout);
    }

  ct->schedule();

  Mword state = ct->state.change(~Thread_full_ipc_mask, Thread_ready);
  ct->reset_timeout();

  if (EXPECT_FALSE(ct->in_sender_list()))
    {
      auto g = lock_guard(_wait_q.lock());
      // Recheck under lock whether thread is still in waiting queue.
      if (ct->in_sender_list())
        {
          ct->sender_dequeue(&_wait_q);

          if (state & Thread_timeout)
            return L4_error::Timeout;

          if (state & Thread_cancel)
            return L4_error::Canceled;
        }
    }

  return L4_error::None;
}

void
Ipc_gate::invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
                 Syscall_frame *f, Utcb *utcb)
{
  //LOG_MSG_3VAL(current(), "gIPC", Mword(_thread), _id, f->obj_2_flags());
  //printf("Invoke: Ipc_gate(%lx->%p)...\n", _id, _thread);
  Thread *ct = current_thread();
  Thread *sender = 0;
  Thread *partner = 0;
  bool have_rcv = false;

  Thread *t = _thread.load(cxx::memory_order_acquire);
  if (EXPECT_FALSE(!t))
    {
      L4_error e = block(ct, f->timeout().snd, utcb);
      if (!e.ok())
        {
          f->tag(commit_error(utcb, e));
          return;
        }

      t = _thread.load(cxx::memory_order_acquire);
      if (EXPECT_FALSE(!t))
        {
          f->tag(commit_error(utcb, L4_error::Not_existent));
          return;
        }
    }

  bool ipc = t->check_sys_ipc(f->ref().op(), &partner, &sender, &have_rcv);

  LOG_TRACE("IPC Gate invoke", "gate", current(), Log_ipc_gate_invoke,
      l->gate_dbg_id = dbg_id();
      l->thread_dbg_id = t->dbg_id();
      l->label = _id.load(cxx::memory_order_relaxed) | cxx::int_value<L4_fpage::Rights>(rights);
  );

  if (EXPECT_FALSE(!ipc))
    f->tag(commit_error(utcb, L4_error::Not_existent));
  else
    {
      Mword const from_spec = _id.load()
                            | cxx::int_value<L4_fpage::Rights>(rights);
      ct->do_ipc(f->tag(), from_spec, partner, have_rcv, sender, f->timeout(), f, rights);
    }
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
ipc_gate_factory(Ram_quota *q, Space *space,
                 L4_msg_tag tag, Utcb const *utcb,
                 int *err)
{
  L4_snd_item_iter snd_items(utcb, tag.words());
  Thread *thread = 0;

  if (tag.items() && snd_items.next())
    {
      L4_fpage bind_thread(snd_items.get()->d);
      *err = L4_err::EInval;
      if (EXPECT_FALSE(!bind_thread.is_objpage()))
        return 0;

      L4_fpage::Rights thread_rights = L4_fpage::Rights(0);
      thread = cxx::dyn_cast<Thread*>(space->lookup_local(bind_thread.obj_index(), &thread_rights));

      if (EXPECT_FALSE(!thread))
        {
          *err = L4_err::ENoent;
          return 0;
        }

      *err = L4_err::EPerm;
      if (EXPECT_FALSE(!(thread_rights & L4_fpage::Rights::CS())))
        return 0;
    }

  *err = L4_err::ENomem;
  return static_cast<Ipc_gate_ctl*>(Ipc_gate::create(q, thread, utcb->values[2]));
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(0, ipc_gate_factory);
  Kobject_iface::set_factory(L4_msg_tag::Label_kobject, ipc_gate_factory);
}
}

#if defined (CONFIG_JDB)

#include "string_buffer.h"

void
Ipc_gate::Log_ipc_gate_invoke::print(String_buffer *buf) const
{
  buf->printf("D-gate=%lx D-thread=%lx L=%lx",
              gate_dbg_id, thread_dbg_id, label);
}

#endif // CONFIG_JDB

