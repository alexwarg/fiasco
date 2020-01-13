
#pragma once

#include <context.h>
#include <types.h>

template<typename T>
class Jdb_tcb_ptr_arch
{
public:
  Address user_ip() const
  {
    return static_cast<T const *>(this)->top_value(-2);
  }

  static bool arch_is_user_value(Address _offs)
  {
    return _offs >= Context::Size - 37 * sizeof(Mword);
  }

  static const char *arch_user_value_desc(Address _offs)
  {
    const char *desc[] =
    {
      "PSR", "PC", "USP", "PFA", "ESR", "KSP", "ULR", "X29", "X28", "X27",
      "X26", "X25", "X24", "X23", "X22", "X21", "X20", "X19", "X18", "X17",
      "X16", "X15", "X14", "X13", "X12", "X11", "X10", "X9", "X8", "X7","X6",
      "X5", "X4", "X3", "X2", "X1", "X0"
    };
    return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
  }
};
