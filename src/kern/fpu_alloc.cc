#include <fpu_alloc.h>

#include "fpu_state.h"
#include "kmem_slab.h"
#include "ram_quota.h"
#include "slab_cache.h"

inline unsigned
quota_offset(unsigned state_size)
{
  return (state_size + alignof(Ram_quota *) - 1) & ~(alignof(Ram_quota *) - 1);
}

static Kmem_slab _fpu_state_allocator(
  quota_offset(Fpu::state_size()) + sizeof(Ram_quota *),
  Fpu::state_align(), "Fpu state");

bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state *s)
{
  unsigned long sz = Fpu::state_size();
  void *b;

  if (!(b = _fpu_state_allocator.q_alloc(q)))
    return false;

  *((Ram_quota **)((char*)b + quota_offset(sz))) = q;
  s->_state_buffer = b;
  Fpu::init_state(s);

  return true;
}

void
Fpu_alloc::free_state(Fpu_state *s)
{
  if (s->_state_buffer)
    {
      unsigned long sz = Fpu::state_size();
      Ram_quota *q = *((Ram_quota **)((char*)(s->_state_buffer)
                                      + quota_offset(sz)));
      _fpu_state_allocator.q_free(q, s->_state_buffer);
      s->_state_buffer = 0;
    }
}
