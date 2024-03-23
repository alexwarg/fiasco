
#pragma once

#include "l4_types.h"
#include "globalconfig.h"


#if defined (CONFIG_ARM_V6PLUS)
namespace Utcb_support {
  inline void current(User<Utcb>::Ptr const &) {}
}
#else  // CONFIG_ARM_V6PLUS

#include "mem_layout.h"

namespace Utcb_support {
  inline void current(User<Utcb>::Ptr const &utcb)
  { *reinterpret_cast<User<Utcb>::Ptr*>(Mem_layout::Utcb_ptr_page) = utcb; }
}
#endif // CONFIG_ARM_V6PLUS

