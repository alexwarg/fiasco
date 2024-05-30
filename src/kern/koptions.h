#pragma once

#include "koptions-def.h"
#include "std_macros.h"

namespace Koptions {
  using namespace L4_kernel_options;

  Options *o() FIASCO_CONST;
}

