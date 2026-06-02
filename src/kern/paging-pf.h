#pragma once

#include <paging-pf-arch.h>
#include <types.h>

namespace PF
{
  inline Mword pc_to_msgword1(Address pc, Mword error)
  {
    return is_usermode_error(error) ? pc : static_cast<Mword>(-1UL);
  }
}

