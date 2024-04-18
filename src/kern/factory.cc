#include "factory.h"

#include "kmem_slab.h"
#include "static_init.h"
#include "logdefs.h"
#include "l4_buf_iter.h"
#include "kobject_rpc.h"
#include "map_util_objs.h"


JDB_DEFINE_TYPENAME(Factory, "\033[33;1mFactory\033[m");

static Factory INIT_PRIORITY(ROOT_FACTORY_INIT_PRIO) _root_factory{true};
static Kmem_slab_t<Factory> _factory_allocator("Factory");

Factory::Self_alloc *
Factory::allocator()
{
  return _factory_allocator.slab();
}

void Factory::operator delete (void *_f)
{
  Factory *f = (Factory*)_f;
  LOG_TRACE("Factory delete", "fa del", ::current(), Tb_entry_empty, {});

  if (!f->parent())
    return;

  Ram_quota *p = f->parent();
  auto limit = f->limit();
  asm ("" : "=m"(*f));

  allocator()->free(f);
  if (p)
    p->free(sizeof(Factory) + limit);
}

L4_msg_tag
Factory::map_obj(Kobject_iface *o, Cap_index cap, Task *_c_space,
                 Obj_space *o_space, Utcb const *utcb)
{
  // must be before the lock guard
  Ref_ptr<Task> c_space(_c_space);
  Reap_list rl;

  auto space_lock_guard = lock_guard_dont_lock(c_space->existence_lock);

  // We take the existence_lock for syncronizing maps...
  // This is kind of coarse grained
  // try_lock fails if the lock is neither locked nor unlocked
  if (!space_lock_guard.check_and_lock(&c_space->existence_lock))
    {
      delete o;
      return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));
    }

  if (!map(o, o_space, c_space.get(), cap, rl.list()))
    {
      delete o;
      return commit_result(-L4_err::ENomem);
    }

  // return a tag with one typed item for the returned capability
  return commit_result(0, 0, 1);
}

L4_msg_tag
Factory::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                 Utcb const *utcb, Utcb *utcb_out)
{
  Context *const c_thread = ::current();
  Task *const c_space = static_cast<Task*>(c_thread->space());

  L4_msg_tag tag = f->tag();
  if (!Ko::check_basics(&tag, rights, L4_msg_tag::Label_factory,
                        L4_fpage::Rights::CS()))
    return tag;

  if (EXPECT_FALSE(!ref.have_recv()))
    return commit_result(0);

  L4_fpage buffer(0);

    {
      L4_buf_iter buf(utcb, utcb->buf_desc.obj());
      L4_buf_iter::Item const *const b = buf.get();
      if (EXPECT_FALSE(b->b.is_void()
                       || b->b.type() != L4_msg_item::Map))
        return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));

      buffer = L4_fpage(b->d);
    }

  if (EXPECT_FALSE(!buffer.is_objpage()))
    return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));

  Kobject_iface *new_o;
  int err = L4_err::ENomem;

  auto cpu_lock_guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  new_o = Kobject_iface::manufacture((long)access_once(utcb->values + 0),
                                     this, c_space, f->tag(), utcb, &err);

  LOG_TRACE("Kobject create", "new", ::current(), Log_entry,
    l->op = utcb->values[0];
    l->buffer = buffer.obj_index();
    l->id = dbg_info()->dbg_id();
    l->ram = current();
    l->newo = new_o ? new_o->dbg_info()->dbg_id() : ~0);

  if (new_o)
    {
      utcb_out->values[0] = (0 << 6) | (L4_fpage::Obj << 4) | L4_msg_item::Map;
      return map_obj(new_o, buffer.obj_index(), c_space, c_space, utcb);
    }
  else
    return commit_result(-err);
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
factory_factory(Ram_quota *q, Space *,
                L4_msg_tag, Utcb const *u,
                int *err)
{
  *err = L4_err::ENomem;
  return static_cast<Factory*>(q)->create_factory(u->values[2]);
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_factory, factory_factory);
}
}

