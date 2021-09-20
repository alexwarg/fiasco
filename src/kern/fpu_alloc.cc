#include <fpu_alloc.h>
#include <fpu.h>

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
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s)
{
  unsigned long sz = Fpu::state_size();
  void *b;

  if (!(b = _fpu_state_allocator.q_alloc(q)))
    return false;

  *((Ram_quota **)((char*)b + quota_offset(sz))) = q;
  s.set(reinterpret_cast<Fpu_state *>(b));
  Fpu::init_state(s.get());

  return true;
}

void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (!s)
    return;

  auto *sb = s.reset();
  unsigned long sz = Fpu::state_size();
  Ram_quota *q = *((Ram_quota **)((char*)(sb)
                                  + quota_offset(sz)));
  _fpu_state_allocator.q_free(q, sb);
}
