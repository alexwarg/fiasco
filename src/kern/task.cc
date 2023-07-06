
#include "task.h"
#include "task_factory_impl.h"

#include "kobject_rpc.h"
#include "kmem_slab.h"
#include "paging.h"
#include "logdefs.h"
#include "kdb_ke.h"
#include "map_util.h"

#include "globalconfig.h"


JDB_DEFINE_TYPENAME(Task, "\033[31mTask\033[m");
static Kmem_slab_t<Task::Ku_mem> _k_u_mem_list_alloc("Ku_mem");
Slab_cache *Space::Ku_mem::a = _k_u_mem_list_alloc.slab();

[[gnu::weak]] bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{ return false; }

int
Task::alloc_ku_mem_chunk(User<void>::Ptr u_addr, unsigned size, void **k_addr)
{
  assert ((size & (size - 1)) == 0);

  Kmem_alloc *const alloc = Kmem_alloc::allocator();
  void *p = alloc->q_alloc(ram_quota(), Bytes(size));

  if (EXPECT_FALSE(!p))
    return -L4_err::ENomem;

  // clean up utcbs
  memset(p, 0, size);

  Virt_addr base((Address)p);
  Mem_space::Page_order page_size(Config::PAGE_SHIFT);

  // the following works because the size is a power of two
  // and once we have size larger than a super page we have
  // always multiples of superpages
  if (size >= Config::SUPERPAGE_SIZE)
    page_size = Mem_space::Page_order(Config::SUPERPAGE_SHIFT);

  for (Virt_size i = Virt_size(0); i < Virt_size(size);
       i += Virt_size(1) << page_size)
    {
      Virt_addr kern_va = base + i;
      Virt_addr user_va = Virt_addr((Address)u_addr.get()) + i;
      Mem_space::Phys_addr pa(pmem_to_phys(cxx::int_value<Virt_addr>(kern_va)));

      // must be valid physical address
      assert(pa != Mem_space::Phys_addr(~0UL));

      Mem_space::Status res =
        static_cast<Mem_space*>(this)->v_insert(pa, user_va, page_size,
            Mem_space::Attr(L4_fpage::Rights::URW()));

      switch (res)
        {
        case Mem_space::Insert_ok: break;
        case Mem_space::Insert_err_nomem:
          free_ku_mem_chunk(p, u_addr, size, cxx::int_value<Virt_size>(i));
          return -L4_err::ENomem;

        case Mem_space::Insert_err_exists:
          free_ku_mem_chunk(p, u_addr, size, cxx::int_value<Virt_size>(i));
          return -L4_err::EExists;

        default:
          printf("UTCB mapping failed: va=%p, ph=%p, res=%d\n",
                 (void *)user_va, (void *)kern_va, res);
          kdb_ke("BUG in utcb allocation");
          free_ku_mem_chunk(p, u_addr, size, cxx::int_value<Virt_size>(i));
          return 0;
        }
    }

  *k_addr = p;
  return 0;
}

int
Task::alloc_ku_mem(L4_fpage ku_area)
{
  if (ku_area.order() < Config::PAGE_SHIFT || ku_area.order() > 20)
    return -L4_err::EInval;

  Mword sz = 1UL << ku_area.order();

  if (ku_area.mem_address() > Virt_addr(Mem_space::user_max() - sz + 1))
    return -L4_err::EInval;

  Ku_mem *m = new (ram_quota()) Ku_mem();

  if (!m)
    return -L4_err::ENomem;

  User<void>::Ptr u_addr((void *)ku_area.mem_address());

  void *p = 0;
  if (int e = alloc_ku_mem_chunk(u_addr, sz, &p))
    {
      m->free(ram_quota());
      return e;
    }

  m->u_addr = u_addr;
  m->k_addr = p;
  m->size = sz;

  _ku_mem.atomic_add(m);

  return 0;
}

inline void
Task::free_ku_mem(Ku_mem *m)
{
  free_ku_mem_chunk(m->k_addr, m->u_addr, m->size, m->size);
  m->free(ram_quota());
}

void
Task::free_ku_mem_chunk(void *k_addr, User<void>::Ptr u_addr, unsigned size,
                        unsigned mapped_size)
{

  Kmem_alloc * const alloc = Kmem_alloc::allocator();
  Mem_space::Page_order page_size(Config::PAGE_SHIFT);

  // the following works because the size is a power of two
  // and once we have size larger than a super page we have
  // always multiples of superpages
  if (size >= Config::SUPERPAGE_SIZE)
    page_size = Mem_space::Page_order(Config::SUPERPAGE_SHIFT);

  for (Virt_size i = Virt_size(0); i < Virt_size(mapped_size);
       i += Virt_size(1) << page_size)
    {
      Virt_addr user_va = Virt_addr((Address)u_addr.get()) + i;
      static_cast<Mem_space*>(this)->v_delete(user_va, page_size, L4_fpage::Rights::FULL());
    }

  alloc->q_free(ram_quota(), Bytes(size), k_addr);
}

void
Task::free_ku_mem()
{
  while (Ku_mem *m = _ku_mem.pop_front())
    free_ku_mem(m);
}

void
Task::operator delete (void *ptr) noexcept
{
  Task *t = reinterpret_cast<Task*>(ptr);
  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
            l->id = t->dbg_id();
            l->obj = t;
            l->type = cxx::Typeid<Task>::get();
            l->ram = t->ram_quota()->current());

  Kmem_slab_t<Task>::q_free(t->ram_quota(), ptr);
}

void
Task::destroy(Kobject ***reap_list)
{
  Kobject::destroy(reap_list);

  fpage_unmap(this, L4_fpage::all_spaces(L4_fpage::Rights::FULL()), L4_map_mask::full(), reap_list);
}

/**
 * \brief Shutdown the task.
 *
 * Currently:
 * -# Unbind and delete all contexts bound to this task.
 * -# Unmap everything from all spaces.
 * -# Delete child tasks.
 */
inline L4_msg_tag
Task::sys_map(L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  LOG_TRACE("Task map", "map", ::current(), Log_map_unmap,
      l->id = dbg_id();
      l->map   = true;
      l->mask  = utcb->values[1];
      l->fpage = utcb->values[2]);

  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CW())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag tag = f->tag();

  L4_fpage::Rights mask;
  Task *_from = Ko::deref<Task>(&tag, utcb, &mask);
  if (!_from)
    return tag;

  L4_fpage sfp(utcb->values[2]);

  if (sfp.type() == L4_fpage::Obj)
    {
      // handle Rights::CS() bit masking for capabilities
      mask &= rights;
      mask |= L4_fpage::Rights::CD() | L4_fpage::Rights::CRW();

      // diminish when sending via restricted ipc gates
      sfp.mask_rights(mask);
    }

  Kobject::Reap_list rl;
  L4_error ret;

    {
      Ref_ptr<Task> from(_from);
      Ref_ptr<Task> self(this);
      // enforce lock order to prevent deadlocks.
      // always take lock from task with the lower memory address first
      Lock_guard_2<Lock> guard;

      // FIXME: avoid locking the current task, it is not needed
      if (!guard.check_and_lock(&existence_lock, &from->existence_lock))
        return commit_result(-L4_err::EInval);

      cpu_lock.clear();

      ret = fpage_map(from.get(), sfp, this,
                      L4_fpage::all_spaces(), L4_msg_item(utcb->values[1]), &rl);
      cpu_lock.lock();
    }

  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();

  // FIXME: treat reaped stuff
  if (ret.ok())
    return commit_result(0);
  else
    return commit_error(utcb, ret);
}

inline L4_msg_tag
Task::sys_unmap(Syscall_frame *f, Utcb *utcb)
{
  Kobject::Reap_list rl;
  unsigned words = f->tag().words();

  LOG_TRACE("Task unmap", "unm", ::current(), Log_map_unmap,
            l->id = dbg_id();
            l->map   = false;
            l->mask  = utcb->values[1];
            l->fpage = utcb->values[2]);

    {
      Ref_ptr<Task> self(this);
      Lock_guard<Lock> guard;

      // FIXME: avoid locking the current task, it is not needed
      if (!guard.check_and_lock(&existence_lock))
        return commit_error(utcb, L4_error::Not_existent);

      cpu_lock.clear();

      L4_map_mask m(utcb->values[1]);

      for (unsigned i = 2; i < words; ++i)
        {
          L4_fpage::Rights const flushed
            = fpage_unmap(this, L4_fpage(utcb->values[i]), m, rl.list());

          utcb->values[i] = (utcb->values[i] & ~0xfUL)
                          | cxx::int_value<L4_fpage::Rights>(flushed);
        }
      cpu_lock.lock();
    }

  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();

  return commit_result(0, words);
}

inline L4_msg_tag
Task::sys_cap_valid(Syscall_frame *, Utcb *utcb)
{
  L4_obj_ref obj(utcb->values[1]);

  if (obj.special())
    return commit_result(0);

  Obj_space::Capability cap = lookup(obj.cap());
  if (EXPECT_TRUE(cap.valid()))
    return commit_result(1);
  else
    return commit_result(0);
}

inline L4_msg_tag
Task::sys_caps_equal(Syscall_frame *, Utcb *utcb)
{
  L4_obj_ref obj_a(utcb->values[1]);
  L4_obj_ref obj_b(utcb->values[2]);

  if (obj_a == obj_b)
    return commit_result(1);

  if (obj_a.special() || obj_b.special())
    return commit_result(obj_a.special_cap() == obj_b.special_cap());

  Obj_space::Capability c_a = lookup(obj_a.cap());
  Obj_space::Capability c_b = lookup(obj_b.cap());

  return commit_result(c_a == c_b);
}

inline L4_msg_tag
Task::sys_add_ku_mem(Syscall_frame *f, Utcb *utcb)
{
  if (EXPECT_FALSE(!(caps() & Task::Caps::kumem())))
    return commit_result(-L4_err::ENosys);

  unsigned const w = f->tag().words();
  for (unsigned i = 1; i < w; ++i)
    {
      L4_fpage ku_fp(utcb->values[i]);
      if (!ku_fp.is_valid() || !ku_fp.is_mempage())
        return commit_result(-L4_err::EInval);

      int e = alloc_ku_mem(ku_fp);
      if (e < 0)
        return commit_result(e);
    }

  return commit_result(0);
}

inline L4_msg_tag
Task::sys_cap_info(Syscall_frame *f, Utcb *utcb)
{
  L4_msg_tag const &tag = f->tag();

  switch (tag.words())
    {
    default: return commit_result(-L4_err::EInval);
    case 2:  return sys_cap_valid(f, utcb);
    case 3:  return sys_caps_equal(f, utcb);
    }
}

void
Task::invoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  if (EXPECT_FALSE(f->tag().proto() != L4_msg_tag::Label_task))
    {
      f->tag(commit_result(-L4_err::EBadproto));
      return;
    }

  switch (utcb->values[0])
    {
    case Map:
      f->tag(sys_map(rights, f, utcb));
      return;
    case Unmap:
      f->tag(sys_unmap(f, utcb));
      return;
    case Cap_info:
      f->tag(sys_cap_info(f, utcb));
      return;
    case Add_ku_mem:
      f->tag(sys_add_ku_mem(f, utcb));
      return;
    default:
      L4_msg_tag tag = f->tag();
      if (invoke_arch(tag, utcb))
        f->tag(tag);
      else
        f->tag(commit_result(-L4_err::ENosys));
      return;
    }
}

namespace {
static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_task,
                             &Task::generic_factory<Task, true, 2>);
}
}


#if defined (CONFIG_JDB)

#include "string_buffer.h"

void
Task::Log_map_unmap::print(String_buffer *buf) const
{
  L4_fpage fp(fpage);
  buf->printf("task=[%c:%lx] %s=%lx fpage=[%u/",
              map ? 'M' : 'U', id,
              map ? "snd_base" : "mask", mask, (unsigned)fp.order());
  switch (fp.type())
    {
    case L4_fpage::Special:
      buf->printf("spc] fpage=%lx", fpage);
      break;
    case L4_fpage::Memory:
      buf->printf("mem] addr=%lx", cxx::int_value<Virt_addr>(fp.mem_address()));
      break;
    case L4_fpage::Io:
      buf->printf("io] port=%lx", cxx::int_value<Port_number>(fp.io_address()));
      break;
    case L4_fpage::Obj:
      buf->printf("obj] cap=C:%lx", cxx::int_value<Cap_index>(fp.obj_index()));
      break;
    default:
      buf->printf("???] fpage=%lx", fpage);
      break;
    }
}

#endif // CONFIG_JDB
