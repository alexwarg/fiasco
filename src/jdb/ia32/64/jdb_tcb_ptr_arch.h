
#pragma once

#include <context.h>
#include <types.h>

template<typename T>
class Jdb_tcb_ptr_arch
{
public:
  Address user_ip() const
  {
    return static_cast<T const *>(this)->top_value(-5);
  }

  static bool arch_is_user_value(Address _offs)
  {
    return _offs >= Context::Size - 5 * sizeof(Mword);
  }

  static const char *arch_user_value_desc(Address _offs)
  {
    const char *desc[] = { "SS", "SP", "RFL", "CS", "IP" };
    return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
  }
};
