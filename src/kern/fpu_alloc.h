#pragma once

#include <fpu_state.h>

class Ram_quota;

namespace Fpu_alloc
{
  bool alloc_state(Ram_quota *q, Fpu_state *s);
  void free_state(Fpu_state *s);
}

