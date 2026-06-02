#pragma once

#include <l4_types.h>
#include <space.h>

#include <cpu_lock.h>
#include <cassert>

class Context_ptr
{
public:
  explicit constexpr Context_ptr(Cap_index id) : _t(id) {}
  Context_ptr() = default;
  Context_ptr(Context_ptr const &o) = default;
  Context_ptr &operator = (Context_ptr const &o) = default;

  [[gnu::nonnull(2, 3)]]
  Kobject_iface *ptr(Space *s, L4_fpage::Rights *rights) const noexcept
  {
    assert (cpu_lock.test());

    return nonull_static_cast<Obj_space*>(s)->lookup_local(_t, rights);
  }


  bool is_valid() const { return _t != Cap_index(~0UL); }

  // only for debugging use
  Cap_index raw() const { return _t; }

private:
  Cap_index _t;
};

template< typename T >
class Context_ptr_base : public Context_ptr
{
public:
  enum Invalid_type { Invalid };
  enum Null_type { Null };
  explicit Context_ptr_base(Invalid_type) noexcept
  : Context_ptr(Cap_index(~0UL))
  {}

  explicit Context_ptr_base(Null_type) noexcept
  : Context_ptr(Cap_index(0))
  {}

  explicit Context_ptr_base(Cap_index id) noexcept
  : Context_ptr(id)
  {}

  Context_ptr_base() = default;

  Context_ptr_base(Context_ptr_base const &o) = default;

  template< typename X >
  Context_ptr_base(Context_ptr_base<X> const &o) noexcept
  : Context_ptr(o)
  { X*x = 0; T*t = x; (void)t; }

  Context_ptr_base &operator = (Context_ptr_base const &o) noexcept = default;

  template< typename X >
  Context_ptr_base &operator = (Context_ptr_base<X> const &o) noexcept
  { X*x=0; T*t=x; (void)t; Context_ptr::operator = (o); return *this; }
};

