#include <fpu_alloc.h>
#include <fpu.h>
#include <globalconfig.h>

#ifndef CONFIG_FPU_ALLOC_TYPED

#include <kmem_slab.h>
#include <ram_quota.h>
#include <slab_cache.h>

constexpr unsigned
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
  void *b;
  if (!(b = _fpu_state_allocator.q_alloc(q)))
    return false;

  Fpu_state *state = new (b) Fpu_state();
  unsigned sz = Fpu::state_size();
  *((Ram_quota **)((char*)state + quota_offset(sz))) = q;
  s.set(state);
  return true;
}

void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (!s)
    return;

  auto *sb = s.reset();
  unsigned sz = Fpu::state_size();
  Ram_quota *q = *((Ram_quota **)((char*)(sb)
                                  + quota_offset(sz)));
  _fpu_state_allocator.q_free(q, sb);
}

#else

class Ram_quota;

namespace Fpu_alloc
{
  Fpu_state *alloc_types_state(Ram_quota *q, Fpu::State_type);
}

bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s, Fpu::State_type type)
{
  Fpu_state *state = alloc_types_state(q, type);
  if (!state)
    return false;

  s.set(state);
  return true;
}

void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  delete s.reset();
}

#endif
