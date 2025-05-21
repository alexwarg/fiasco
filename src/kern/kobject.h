#pragma once

#include "lock.h"
#include "obj_space.h"
#include "context.h"
#include "kobject_dbg.h"
#include "kobject_iface.h"
#include "lock_guard.h"
#include "l4_error.h"
#include "rcupdate.h"
#include "space.h"
#include "logdefs.h"

#include <cxx/hlist>

#ifdef CONFIG_JDB
#include "tb_entry.h"
#include "string_buffer.h"
#endif


class Kobject_mappable
{
private:
  friend class Kobject_mapdb;
  friend class Jdb_mapdb;
  friend class Obj_mapdb_test;

  Obj::Mapping::List _root;
  Smword _cnt;
  Lock _lock;

public:
  Kobject_mappable() : _cnt(0) {}
  bool no_mappings() const { return _root.empty(); }

  /**
   * Insert the root mapping of an object.
   */
  void insert_root_mapping(Obj::Cap_addr &m)
  {
    m._c->put_as_root();
    _root.add(m._c);
    _cnt = 1;
  }

  Smword dec_cap_refcnt(Smword diff)
  {
    auto g = lock_guard(_lock);
    _cnt -= diff;
    return _cnt;
  }


private:
  void invalidate_mappings()
  {
    assert(_lock.test());

    for (auto const &&i: _root)
      {
        Obj::Entry *e = static_cast<Obj::Entry*>(i);
        if (e->is_ref_counted())
          --_cnt;
        e->invalidate();
      }

    _root.clear();
  }
};

/**
 * Basic kernel object.
 *
 * This is the base class for all kernel objects that are exposed through
 * capabilities.
 *
 * Kernel objects have a specific life-cycle management that is based
 * on (a) kernel external visibility through capabilities and (b) kernel
 * internal references among kernel objects.
 *
 * If there are no more capabilities to a kernel object, due to an unmap
 * (either of the last capability, or with delete flag and delete rights),
 * the kernel runs the 2-phase destruction protocol for kernel objects
 * (implemented in Kobject::Reap_list::del()):
 * - Call Kobject::destroy(). Mark this object as invalid. When this function
 *   returns, the object cannot be longer referenced. One object reference is
 *   still maintained to prevent the final removal before all other objects
 *   realized that the object vanished.
 * - Block for an RCU grace period.
 * - Call Kobject::put(). Release the final reference to the object preparing
 *   the object deletion.
 *
 * If Kobject::put() returns true, the kernel object is to be deleted.
 * If there are any other non-capability references to a kernel object,
 * Kobject::put() *must* return `false` and an object-specific MP-safe
 * mechanism for object deletions has to take over.
 *
 * Kobject::destroy() and Kobject::put() are called exactly once as a direct
 * consequence of unmapping. Usually, during the lifetime of a `Kobject`,
 * there are no additional calls to those functions; however, object-specific
 * reference handling may allow or require additional calls.
 */
class Kobject :
  public cxx::Dyn_castable<Kobject, Kobject_iface>,
  private Kobject_mappable,
  private Kobject_dbg
{
  template<typename T, bool>
  friend class Map_traits;

public:
  using Dyn_castable<Kobject, Kobject_iface>::_cxx_dyn_type;

  class Reap_list
  {
  private:
    Kobject *_h = nullptr;
    Kobject **_t = &_h;

  public:
    Reap_list() = default;
    ~Reap_list() { del(); }
    Kobject ***list() { return &_t; }
    bool empty() const { return _h == nullptr; }
    void del_1()
    {
      for (Kobject *reap = _h; reap; reap = reap->_next_to_reap)
        reap->destroy(list());
    }

    void del_2()
    {
      for (Kobject *reap = _h; reap;)
        {
          Kobject *d = reap;
          reap = reap->_next_to_reap;
          if (d->put())
            delete d;
        }

      _h = nullptr;
      _t = &_h;
    }

    /**
     * Delete kernel objects without capability references.
     */
    void del()
    {
      if (EXPECT_TRUE(empty()))
        return;

      del_1();
      current()->rcu_wait();
      del_2();
    }
  };

  /**
   * RAII smart pointer to an existence locked Kobject.
   *
   * The pointer is either initialized from a reference counted pointer, or from a
   * function-like that safely retuns a Kobject pointer that stays valid until
   * the next preemption point. If a function-like is used it shall be reevaluated
   * after each preeption point, to make sure the object is still alive.
   * If a nullptr is returned from the function-like, or the existence_lock is marked
   * invalid, the smart pointer shall be nullptr / invalid.
   * If the pointer is valid, the objects existence lock is held, and will be released
   * in the destructor.
   */
  template<typename T>
  class Locked
  {
  private:
    T *_o = nullptr;

  public:
    Locked(Locked const &) = delete;
    void operator = (Locked const &) = delete;

    constexpr Locked() noexcept = default;
    constexpr Locked(Locked &&o) noexcept : _o(o.release()) {}
    constexpr Locked &operator = (Locked &&o) noexcept
    {
      if (&o != this)
        _o = o.release();

      return *this;
    }

    constexpr T *release()
    {
      T *tmp = _o;
      _o = nullptr;
      return tmp;
    }

    ~Locked() noexcept
    {
      if (_o)
        _o->existence_lock.clear();
      _o = nullptr;
    }

    /**
     * initialize from frunction-like ref.
     * The ref will be called before trying to take the exisence lock, and must
     * always return a valid object pointer or nullptr. after each possible
     * preemption point ref will be called again.
     */
    template<typename REF>
    explicit Locked(REF &&ref)
    : _o(Lock::try_lock(ref, [](T *o){ return &o->existence_lock; }))
    {}

    /**
     * initialize from Ref_ptr<T>.
     * The pointer is assumed to stay valid until the lock is taken, or the object
     * is marked invalid. (Note, there is no reevaluation needed)
     */
    explicit Locked(Ref_ptr<T> const &o)
    {
      if (o->existence_lock.lock() == Lock::Invalid)
        return;
      _o = o.get();
    }

    constexpr explicit operator bool () const { return _o != nullptr; }
    constexpr T *get() const  { return _o; }
    T &operator * () const { return *_o; }
    T *operator -> () const { return _o; }
  };

  using Kobject_dbg::dbg_id;

  Lock existence_lock;

  bool is_local(Space *) const override
  { return false; }

  Mword obj_id() const override
  { return ~0UL; }

  Kobject_mappable *map_root() override
  { return this; }

  virtual bool put()
  { return true; }

  void initiate_deletion(Kobject ***reap_list) override;

  virtual
  void destroy(Kobject ***)
  {
    LOG_TRACE("Kobject destroy", "des", current(), Log_destroy,
        l->id = dbg_id();
        l->obj = this;
        l->type = cxx::dyn_typeid(this));
    existence_lock.wait_free();
  }

  virtual ~Kobject() noexcept
  {
    LOG_TRACE("Kobject delete (generic)", "del", current(), Log_destroy,
        l->id = dbg_id();
        l->obj = this;
        l->type = 0);
  }

private:
  Kobject *_next_to_reap;

  L4_msg_tag sys_dec_refcnt(L4_msg_tag tag, Utcb const *in, Utcb *out)
  {
    if (tag.words() < 2)
      return Kobject_iface::commit_result(-L4_err::EInval);

    Smword diff = in->values[1];
    out->values[0] = dec_cap_refcnt(diff);
    return Kobject_iface::commit_result(0, 1);
  }

public:
  enum Op {
    O_dec_refcnt = 0,
  };

  /**
   * \pre tag.words() >= 1
   */
  L4_msg_tag kobject_invoke(L4_obj_ref, L4_fpage::Rights /*rights*/,
                            Syscall_frame *f,
                            Utcb const *in, Utcb *out)
  {
    L4_msg_tag tag = f->tag();

    switch (in->values[0])
      {
      case O_dec_refcnt:
        return sys_dec_refcnt(tag, in, out);
      default:
        return Kobject_iface::commit_result(-L4_err::ENosys);
      }
  }

#if defined (CONFIG_JDB)
protected:
  struct Log_destroy : public Tb_entry
  {
    Kobject    *obj;
    Mword       id;
    cxx::Type_info const *type;
    Mword       ram;
    void print(String_buffer *buf) const
    {
      buf->printf("obj=%lx [%p] (%p) ram=%lx", id, type, static_cast<void *>(obj), ram);
    }
  };

public:
  static Kobject *from_dbg(Kobject_dbg *d)
  { return static_cast<Kobject*>(d); }

  static Kobject *from_dbg(Kobject_dbg::Iterator const &d)
  {
    if (d != Kobject_dbg::end())
      return static_cast<Kobject*>(*d);
    return 0;
  }

  Kobject_dbg *dbg_info() const override
  { return const_cast<Kobject*>(this); }

#endif // CONFIG_JDB
};

