
#pragma once

#include "l4_types.h"
#include "globalconfig.h"


#if defined (CONFIG_ARM_V6PLUS)
namespace Utcb_support {
  inline void current(User_ptr<Utcb> const &) {}
}
#else  // CONFIG_ARM_V6PLUS

#include "mem_layout.h"

namespace Utcb_support {
  inline void current(User_ptr<Utcb> const &utcb)
  { *reinterpret_cast<User_ptr<Utcb> *>(Mem_layout::Utcb_ptr_page) = utcb; }
}
#endif // CONFIG_ARM_V6PLUS

