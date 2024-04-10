#pragma once

#include <fpu.h>

class Ram_quota;

class Fpu_alloc : public Fpu
{
public:
  static bool alloc_state(Ram_quota *q, Fpu_state *s);
  static void free_state(Fpu_state *s);
};

