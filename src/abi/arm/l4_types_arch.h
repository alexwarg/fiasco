#pragma once

#include "config.h"

namespace L4_exception_ipc {
#if defined (CONFIG_32BIT)
  enum { Msg_size = 21 };
#else
  enum { Msg_size = 39 };
#endif
}
