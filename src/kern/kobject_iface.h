#pragma once

#include "l4_types.h"
#include <cxx/dyn_cast>
#include <cstdint>

class Kobject;
class Kobject_dbg;
class Kobject_mappable;

class Space;
class Ram_quota;
class Syscall_frame;
class Utcb;

class Kobject_common : public cxx::Dyn_castable<Kobject_common>
{
public:
  Kobject_common() = default;
  Kobject_common(Kobject_common const &) = delete;
  Kobject_common &operator = (Kobject_common const &) = delete;

  virtual bool is_local(Space *) const  = 0;
  virtual Mword obj_id() const  = 0;
  virtual void initiate_deletion(Kobject ***) = 0;

  virtual Kobject_mappable *map_root() = 0;
  virtual ~Kobject_common() = 0;

#if defined (CONFIG_JDB)
  virtual Kobject_dbg *dbg_info() const = 0;
#endif
};

class Kobject_iface : public cxx::Dyn_castable<Kobject_iface, Kobject_common>
{
public:

  /**
   * Reevaluable object reference.
   *
   * This is a typed reference to a kernel object that facilitates reevaluation
   * and re-checking of capability references, for example, after preemption points
   * or potential blocking.
   *
   * The reference therefore takes the origin of the object pointer into account.
   * In case the reference is comming from a special capability (reply cap, or self)
   * there is no need and no possibility to reevaluate the reference.
   * However, if the reference comes from a real capability lookup the lookup is
   * repeated in the eval() function and rights must be the same as before the lookup.
   */
  template<typename T, typename OSPC>
  class Typed_ref
  {
  private:
    uintptr_t _obj_or_space;
    L4_obj_ref _ref;

    constexpr T *_obj() const { return reinterpret_cast<T *>(_obj_or_space & ~3ul); }
    constexpr unsigned char _rights() const { return _obj_or_space & 3ul; }
    constexpr OSPC *_space() const { return reinterpret_cast<OSPC *>(_obj_or_space & ~3ul); }

    static constexpr bool is_obj_ptr(L4_obj_ref ref)
    { return (ref.op() & L4_obj_ref::Ipc_reply) || (ref.special() && ref.self()); }

  public:
    constexpr bool is_obj_ptr() const
    { return is_obj_ptr(_ref); }

    constexpr Typed_ref(T *obj, L4_obj_ref ref, L4_fpage::Rights r, OSPC *space)
    : _obj_or_space(is_obj_ptr(ref)
        ? reinterpret_cast<uintptr_t>(obj) | (cxx::int_value<L4_fpage::Rights>(r) & 3ul)
        : reinterpret_cast<uintptr_t>(space) | (cxx::int_value<L4_fpage::Rights>(r) & 3ul)),
      _ref(ref)
    {}

    T *reeval() const
    {
      if (is_obj_ptr())
        return _obj();

      return _space()->lookup_local(_ref.cap(), L4_fpage::Rights(_rights())).template deref<T>();
    }

    T* operator () () const { return reeval(); }
  };

  ///
  // convenientlty get a typed reference from invoke arguments.
  // example: auto reref = get_ref(this, self, rights, current()->space());
  template<typename T, typename OSPC>
  static constexpr Typed_ref<T, OSPC>
  get_ref(T *obj, L4_obj_ref ref, L4_fpage::Rights r, OSPC *space)
  { return Typed_ref<T, OSPC>(obj, ref, r, space); }


  virtual void invoke(L4_obj_ref self, L4_fpage::Rights rights, Syscall_frame *, Utcb *) = 0;

  typedef Kobject_iface *Factory_func(Ram_quota *q,
                                      Space *current_space,
                                      L4_msg_tag tag,
                                      Utcb const *utcb, int *err);
  enum { Max_factory_index = -L4_msg_tag::Max_factory_label };
  static Factory_func *factory[Max_factory_index + 1];

  static
  L4_msg_tag commit_result(Mword error,
                           unsigned words = 0, unsigned items = 0)
  {
    return L4_msg_tag(words, items, 0, error);
  }

  static
  L4_msg_tag commit_error(Utcb const *utcb, L4_error const &e,
                          L4_msg_tag const &tag = L4_msg_tag(0, 0, 0, 0))
  {
    const_cast<Utcb*>(utcb)->error = e;
    return L4_msg_tag(tag, L4_msg_tag::Error);
  }

  virtual Kobject_iface *downgrade(unsigned long del_attribs)
  { (void)del_attribs; return this; }

  static
  Kobject_iface *manufacture(long label, Ram_quota *q,
                             Space *current_space,
                             L4_msg_tag tag, Utcb const *utcb, int *err)
  {
    *err = L4_err::ENodev;
    if (EXPECT_FALSE(label > 0 || -label > Max_factory_index
                     || !factory[-label]))
      return 0;

    return factory[-label](q, current_space, tag, utcb, err);
  }

  static void set_factory(long label, Factory_func *f);
};

inline Kobject_common::~Kobject_common() {}

