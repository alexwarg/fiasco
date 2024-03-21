#pragma once

#include <cxx/atomic>

#include "globalconfig.h"
#include "kobject.h"
#include "kobject_helper.h"
#include "slab_cache.h"
#include "assert_opt.h"

#if defined (CONFIG_JDB)
#include "tb_entry.h"
#endif // CONFIG_JDB

#include <cstddef>

class Ram_quota;

class Ipc_gate_obj;

class Ipc_gate_ctl : public Kobject_h<Ipc_gate_ctl, Kobject_iface>
{
public:
  Kobject_iface *downgrade(unsigned long attr) override;

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  L4_msg_tag kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out);

private:
  enum Operation
  {
    Op_bind     = 0x10,
    Op_get_info = 0x11,
  };

  L4_msg_tag bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                         Syscall_frame *f, Utcb const *in, Utcb *);

  L4_msg_tag get_infos(L4_obj_ref, L4_fpage::Rights,
                       Syscall_frame *, Utcb const *, Utcb *out);
};

class Ipc_gate : public cxx::Dyn_castable<Ipc_gate, Kobject>
{
  friend class Ipc_gate_ctl;
  friend class Jdb_sender_list;

public:
  static Ipc_gate_obj *create(Ram_quota *q, Thread *t, Mword id);

  Ipc_gate(Ram_quota *q, Thread *t, Mword id)
  : _thread(0), _id(id), _quota(q), _wait_q()
  {
    if (t)
      {
        t->inc_ref();
        _thread.store(t);
      }
  }

  void invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

private:
  L4_error block(Thread *ct, L4_timeout const &to, Utcb *u);

protected:
  cxx::atomic<Thread *> _thread;
  cxx::atomic<Mword> _id;

  Ram_quota *_quota;
  Locked_prio_list _wait_q;

#if defined (CONFIG_JDB)
  struct Log_ipc_gate_invoke : public Tb_entry
  {
    Mword gate_dbg_id;
    Mword thread_dbg_id;
    Mword label;
    void print(String_buffer *buf) const;
  };
#endif // CONFIG_JDB
};

class Ipc_gate_obj :
  public cxx::Dyn_castable<Ipc_gate_obj, Ipc_gate, Ipc_gate_ctl>
{
  friend class Ipc_gate;
  typedef Slab_cache Self_alloc;

  static Self_alloc *allocator();

public:
  Ipc_gate_obj(Ram_quota *q, Thread *t, Mword id)
  : Dyn_castable_class(q, t, id)
  {}

  bool put() override { return Ipc_gate::put(); }

  Thread *thread() const { return _thread.load(cxx::memory_order_relaxed); }
  Mword id() const { return _id.load(cxx::memory_order_relaxed); }
  Mword obj_id() const override { return id(); }

  bool is_local(Space *s) const override
  {
    Thread *t = _thread.load(cxx::memory_order_acquire);
    return t && t->space() == s;
  }

  ::Kobject_mappable *map_root() override
  { return Ipc_gate::map_root(); }

  void unblock_all();
  void initiate_deletion(Kobject ***r) override;
  void destroy(Kobject ***r) override;

  ~Ipc_gate_obj() noexcept
  {
    unblock_all();
  }

  void *operator new (size_t, void *b) noexcept
  { return b; }

  void operator delete (void *_f) noexcept;

#if defined (CONFIG_JDB)
  ::Kobject_dbg *dbg_info() const override
  { return Ipc_gate::dbg_info(); }
#endif // CONFIG_JDB
};

