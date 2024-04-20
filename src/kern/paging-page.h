#pragma once

#include <paging-page-arch.h>
#include <types.h>
#include <l4_msg_item.h>
#include <l4_fpage.h>

namespace Page
{
 /* These things must be defined in arch part in
     the most efficent way according to the architecture.

  enum Attribs_enum {
    USER_NO  = xxx, ///< User No access
    USER_RO  = xxx, ///< User Read only
    USER_RW  = xxx, ///< User Read/Write
    USER_RX  = xxx, ///< User Read/Execute
    USER_XO  = xxx, ///< User Execute only
    USER_RWX = xxx, ///< User Read/Write/Execute

    NONCACHEABLE = xxx, ///< Caching is off
    CACHEABLE    = xxx, ///< Cahe is enabled
  };

  */

  typedef L4_msg_item::Memory_type Type;

  struct Kern
  : cxx::int_type_base<unsigned char, Kern>,
    cxx::int_bit_ops<Kern>,
    cxx::int_null_chk<Kern>
  {
    Kern() = default;
    explicit Kern(Value v) : cxx::int_type_base<unsigned char, Kern>(v) {}

    static Kern Global() { return Kern(1); }
  };

  typedef L4_fpage::Rights Rights;

  struct Attr
  {
    Rights rights;
    Type type;
    Kern kern;

    Attr() = default;
    explicit Attr(Rights r, Type t = Type::Normal(), Kern k = Kern(0))
    : rights(r), type(t), kern(k) {}

    Attr apply(Attr o) const
    {
      Attr n = *this;
      n.rights &= o.rights;
      if ((o.type & Type::Set()) == Type::Set())
        n.type = o.type & ~Type::Set();
      return n;
    }

    constexpr bool empty() const
    { return (rights & Rights::RWX()).empty(); }

    Attr operator |= (Attr r)
    {
      rights |= r.rights;
      return *this;
    }
  };
}
