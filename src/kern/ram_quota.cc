
#include "ram_quota.h"

Ram_quota *Ram_quota::root;

bool
Ram_quota::alloc(Mword bytes)
{
  // prevent overflow
  if (bytes >= Invalid)
    return false;

  if (unlimited())
    return true;

  Mword o = _current.load(cxx::memory_order_relaxed);
  for (;;)
    {
      if (o & Invalid)
        return false;

      Mword n = o + bytes;
      if (n > _max)
        return false;

      if (_current.compare_exchange_weak(o, n))
        return true;
    }
}

