
#include "jdb_kobject_names.h"

#include <cstdio>

#include <feature.h>
#include "context.h"
#include "kmem_alloc.h"
#include "minmax.h"
#include "panic.h"
#include "space.h"
#include "thread.h"
#include "static_init.h"


enum
{
  Name_buffer_size = 8192,
  Name_entries = Name_buffer_size / sizeof(Jdb_kobject_name),
};


char const *const Jdb_kobject_name::static_type = "Jdb_kobject_names";
Jdb_kobject_name *Jdb_kobject_name::_names;

static Spin_lock<> allocator_lock;

void *
Jdb_kobject_name::operator new (size_t) throw()
{
  Jdb_kobject_name *n = _names;
  while (1)
    {
      void **o = reinterpret_cast<void**>(n);
      if (!*o)
        {
          auto g = lock_guard(allocator_lock);
          if (!*o)
            {
              *o = (void*)10;
              return n;
            }
        }

      ++n;

      if ((n - _names) >= Name_entries)
        return 0;
    }
}

void
Jdb_kobject_name::operator delete (void *p)
{
  auto g = lock_guard(allocator_lock);
  void **o = reinterpret_cast<void**>(p);
  *o = 0;
}

void
Jdb_kobject_name::name(char const *name, int size)
{
  int i = 0;
  if (size > max_len())
    size = max_len();
  for (; name[i] && i < size; ++i)
    _name[i] = name[i];

  for (; i < max_len(); ++i)
    _name[i] = 0;
}

class Jdb_name_hdl : public Jdb_kobject_handler
{
public:
  bool show_kobject(Kobject_common *, int) override { return true; }
  virtual ~Jdb_name_hdl() {}

  void show_kobject_short(String_buffer *buf, Kobject_common *o,
                          bool dense) override
  {
    Jdb_kobject_name *ex
      = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);

    if (ex)
      buf->printf(" {%-*.*s}",
                  dense ? 0 : ex->max_len(), ex->max_len(), ex->name());
  }

  bool invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb) override
  {
    switch (utcb->values[0])
      {
      case Op_set_name:
          {
            bool enqueue = false;
            Jdb_kobject_name *ne;
            ne = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);
            if (!ne)
              {
                ne = new Jdb_kobject_name();
                if (!ne)
                  {
                    f->tag(Kobject_iface::commit_result(-L4_err::ENomem));
                    return true;
                  }
                enqueue = true;
              }

            if (f->tag().words() > 0)
              ne->name(reinterpret_cast<char const *>(&utcb->values[1]),
                       (f->tag().words() - 1) * sizeof(Mword));
            if (enqueue)
              o->dbg_info()->_jdb_data.add(ne);
            f->tag(Kobject_iface::commit_result(0));
            return true;
          }
      case Op_get_name:
          {
            Kobject_dbg::Iterator o = Kobject_dbg::id_to_obj(utcb->values[1]);
            if (o == Kobject_dbg::end())
              {
                f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
                return true;
              }
            Jdb_kobject_name *n = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(Kobject::from_dbg(o));
            if (!n)
              {
                f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
                return true;
              }

            unsigned l = min<unsigned>(n->max_len(), sizeof(utcb->values) - 1);
            char *dst = reinterpret_cast<char *>(utcb->values);
            strncpy(dst, n->name(), l);
            dst[l] = 0;

            f->tag(Kobject_iface::commit_result(0, (l + 1 + sizeof(Mword) - 1) / sizeof(Mword)));
            return true;
          }
      }
    return false;
  }
};

void FIASCO_INIT
Jdb_kobject_name::init()
{
  _names = (Jdb_kobject_name*)Kmem_alloc::allocator()->alloc(Bytes(Name_buffer_size));
  if (!_names)
    panic("No memory for thread names");

  for (int i=0; i<Name_entries; i++)
    *reinterpret_cast<unsigned long*>(_names + i) = 0;

  static Jdb_name_hdl hdl;
  Jdb_kobject::module()->register_handler(&hdl);
}


STATIC_INITIALIZE(Jdb_kobject_name);
