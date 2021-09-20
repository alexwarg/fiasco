#pragma once

#include <fpu_state_ptr.h>

class Ram_quota;

namespace Fpu_alloc
{
  bool alloc_state(Ram_quota *q, Fpu_state_ptr &s);
  void free_state(Fpu_state_ptr &s);
}

