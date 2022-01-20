#pragma once

#include <types.h>

template<typename DERIVED>
class Mem_layout_defaults
{
public:
  static Address hw_user_max()
  { return DERIVED::User_max; }
};
