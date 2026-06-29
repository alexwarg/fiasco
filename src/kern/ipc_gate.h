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

template<typename T, typename ...M>
struct type_in_list : cxx::false_type {};

template<typename T, typename ...M>
struct type_in_list<T, T, M...> : cxx::true_type {};

template<typename T, typename X, typename ...M>
struct type_in_list<T, X, M...> : type_in_list<T, M...> {};

class Ram_quota;
class Ipc_gate_obj;

class Ipc_gate_if : public Kobject_iface
{
public:
  virtual void del(Kobject_iface *) = 0;
  virtual void del_notify() = 0;
};

class Ipc_gate final : public Ipc_gate_if
{
  friend class Ipc_gate_ctl;
  friend class Jdb_sender_list;

public:
  static Ipc_gate_obj *create(Ram_quota *q, Thread *t, Mword id);

  Ipc_gate() = default;
  Ipc_gate(Thread *t, Mword id);

  void invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  bool is_local(Space *s) const override;
  Mword obj_id() const override;
  void initiate_deletion(Kobject***) override;
  Kobject_mappable* map_root() override;
  Kobject_iface* downgrade(long unsigned int) override;
  void del(Kobject_iface *) override;
  void del_notify() override;
#if defined (CONFIG_JDB)
  Kobject_dbg* dbg_info() const override;
#endif // CONFIG_JDB
};

class Ipc_gate_unbound final : public Ipc_gate_if
{
  friend class Ipc_gate_ctl;
  friend class Jdb_sender_list;

public:
  static Ipc_gate_obj *create(Ram_quota *q, Thread *t, Mword id);

  Ipc_gate_unbound() = default;

  void invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  bool is_local(Space*) const override { return false; }
  Mword obj_id() const override { return 0; }
  void initiate_deletion(Kobject***) override;
  Kobject_mappable* map_root() override;
  Kobject_iface* downgrade(long unsigned int) override { return this; }
  void del(Kobject_iface *) override {}
  void del_notify() override {}
#if defined (CONFIG_JDB)
  Kobject_dbg* dbg_info() const override;
#endif // CONFIG_JDB

private:
  L4_error block(Thread *ct, L4_timeout const &to, Utcb *u);
};

template<typename A0>
constexpr A0 xmax(A0 a0)
{ return a0; }

template<typename A0, typename A1, typename ...A>
constexpr A0 xmax(A0 a0, A1 a1, A ... a)
{ A0 m = xmax(a1, a...); return m > a0 ? m : a0; }

template<typename If, typename M1, typename ...M>
struct alignas(xmax(alignof(M1), alignof(M)...)) Poly_type
{
  char _s[xmax(sizeof(M1), sizeof(M)...)];
  operator If * () { return reinterpret_cast<If *>(_s); }
  operator If const * () const { return reinterpret_cast<If const *>(_s); }
  If * operator -> () { return reinterpret_cast<If *>(_s); }
  If const * operator -> () const { return reinterpret_cast<If const *>(_s); }
  If & operator * () { return *reinterpret_cast<If *>(_s); }
  If const & operator * () const { return *reinterpret_cast<If const *>(_s); }
  If *get() { return reinterpret_cast<If *>(_s); }
  If const *get() const { return reinterpret_cast<If const *>(_s); }

  template<typename T, typename = cxx::enable_if_t<type_in_list<T, M1, M...>::value>,
           typename ... Args>
  void construct(Args &&...args)
  {
    new (_s) T(cxx::forward<Args>(args)...);
  }

  template<typename T, typename = cxx::enable_if_t<type_in_list<T, M1, M...>::value>>
  static Poly_type<If, M1, M...> *as_poly(T *t)
  { return reinterpret_cast<Poly_type<If, M1, M...> *>(t); }

  template<typename T, typename = cxx::enable_if_t<type_in_list<T, M1, M...>::value>>
  static Poly_type<If, M1, M...> const *as_poly(T const *t)
  { return reinterpret_cast<Poly_type<If, M1, M...> const *>(t); }

  Poly_type() { construct<M1>(); }
};

using Poly_ipc_gate = Poly_type<Ipc_gate_if, Ipc_gate_unbound, Ipc_gate>;

class Ipc_gate_obj final :
  public cxx::Dyn_castable<Ipc_gate_obj, Kobject_h<Ipc_gate_obj, Kobject>>,
  private Poly_ipc_gate
{
public:
   Kobject_iface *downgrade(unsigned long attr) override;

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  L4_msg_tag kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out);

private:
  friend class Ipc_gate;
  friend class Ipc_gate_unbound;
  friend class Jdb_sender_list;

  friend Ipc_gate_obj *ipc_gate_obj(void *g);
  friend Ipc_gate_obj const *ipc_gate_obj(void const *g);

  Poly_ipc_gate &poly() noexcept { return *this; }
  Poly_ipc_gate const &poly() const noexcept { return *this; }

  template<typename T>
  static Ipc_gate_obj const *from_poly(T const *p)
  { return static_cast<Ipc_gate_obj const *>(as_poly(p)); }

  template<typename T>
  static Ipc_gate_obj *from_poly(T *p)
  { return static_cast<Ipc_gate_obj *>(as_poly(p)); }

  template<typename T>
  static Thread *target_thread(T const *p)
  {
    return static_cast<Thread *>(from_poly(p)->_tgt.load(cxx::memory_order_acquire));
  }


  enum Operation
  {
    Op_bind     = 0x10,
    Op_get_info = 0x11,
  };

  L4_msg_tag bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                         Syscall_frame *f, Utcb const *in, Utcb *);

  L4_msg_tag get_infos(L4_obj_ref, L4_fpage::Rights,
                       Syscall_frame *, Utcb const *, Utcb *out);


  typedef Slab_cache Self_alloc;

  static Self_alloc *allocator();

  cxx::atomic<Thread *> _tgt;
  cxx::atomic<Mword> _id;

  Ram_quota *_quota;
  Locked_prio_list _wait_q;

public:
  Ipc_gate_obj(Ram_quota *q, Thread *t, Mword id)
  : _quota(q)
  {
    if (t)
      poly().construct<Ipc_gate>(t, id);
  }

  //bool put() override { return Ipc_gate_ctl::put(); }

  Thread *thread() const { return _tgt.load(cxx::memory_order_relaxed); }
  Mword id() const { return _id.load(cxx::memory_order_relaxed); }
  Mword obj_id() const override { return id(); }
  bool is_local(Space *s) const override { return poly()->is_local(s); }

  //::Kobject_mappable *map_root() override
  //{ return Kobject::map_root(); }

  void unblock_all(bool abort = false);
  void initiate_deletion(Kobject ***r) override;
  void destroy(Kobject ***r) override;

  ~Ipc_gate_obj() noexcept
  {
    unblock_all(true);
  }

  void *operator new (size_t, void *b) noexcept
  { return b; }

  void operator delete (void *_f) noexcept;

#if defined (CONFIG_JDB)
  ::Kobject_dbg *dbg_info() const override
  { return Kobject::dbg_info(); }

  struct Log_ipc_gate_invoke : public Tb_entry
  {
    Mword gate_dbg_id;
    Mword thread_dbg_id;
    Mword label;
    void print(String_buffer *buf) const;
  };
#endif // CONFIG_JDB
};

