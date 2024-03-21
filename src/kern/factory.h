#pragma once

#include "fiasco_defs.h"
#include "ram_quota.h"
#include "slab_cache.h"
#include "kobject_helper.h"

#if defined (CONFIG_JDB)

#include "tb_entry.h"
#include "string_buffer.h"

#endif // CONFIG_JDB

class Factory : public Ram_quota, public Kobject_h<Factory>
{
  typedef Slab_cache Self_alloc;

public:
  /// root factory ctor with 'bool' arg
  explicit Factory(bool) noexcept : Ram_quota(true) {}
  Factory(Ram_quota *q, Mword max) noexcept : Ram_quota(q, max) {}

  void operator delete (void *_f);

  [[gnu::pure]]
  static Factory *root()
  { return nonull_static_cast<Factory*>(Ram_quota::root); }

  void destroy(Kobject ***rl) override
  {
    Kobject::destroy(rl);
    take_and_invalidate();
  }

  bool put() override
  {
    return Ram_quota::put();
  }

  Factory *create_factory(Mword max)
  {
    if (!check_max(max))
      return 0;

    Auto_quota<Ram_quota> q(this, sizeof(Factory) + max);
    if (EXPECT_FALSE(!q))
      return 0;

    void *nq = allocator()->alloc();
    if (EXPECT_FALSE(!nq))
      return 0;

    q.release();
    return new (nq) Factory(this, max);
  }

  L4_msg_tag kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                     Utcb const *utcb, Utcb *utcb_out);


private:
  static Self_alloc *allocator();

  L4_msg_tag map_obj(Kobject_iface *o, Cap_index cap, Task *_c_space,
                     Obj_space *o_space, Utcb const *utcb);

#if defined (CONFIG_JDB)
  struct Log_entry : public Tb_entry
  {
    Smword op;
    Cap_index buffer;
    Mword id;
    Mword ram;
    Mword newo;
    void print(String_buffer *buf) const
    {
      static char const *const ops[] =
      { /*   0 */ "gate", "irq", 0, 0, 0, 0, 0, 0,
        /*  -8 */ 0, 0, 0, "task", "thread", 0, 0, "factory",
        /* -16 */ "vm", "dmaspace", "irqsender", 0, "sem" };
      char const *_op = -op < (int)(sizeof(ops) / sizeof(ops[0]))
        ? ops[-op] : "invalid op";
      if (!_op)
        _op = "(nan)";

      buf->printf("factory=%lx [%s] new=%lx cap=[C:%lx] ram=%lx",
                  id, _op, newo, cxx::int_value<Cap_index>(buffer), ram);
    }
  };
#endif // CONFIG_JDB
};

