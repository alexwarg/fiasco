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

template<typename ALLOC>
inline bool _alloc_state(Ram_quota *q, Fpu_state_ptr &s, unsigned long sz, ALLOC &&alloc)
{
  void *b;

  if (!(b = alloc.q_alloc(q)))
    return false;

  *((Ram_quota **)((char*)b + quota_offset(sz))) = q;
  s.set(reinterpret_cast<Fpu_state *>(b));

  return true;
}

template<typename ALLOC>
inline void _free_state(Fpu_state_ptr &s, unsigned long sz, ALLOC &&alloc)
{
  auto *sb = s.reset();
  Ram_quota *q = *((Ram_quota **)((char*)(sb)
                                  + quota_offset(sz)));
  alloc.q_free(q, sb);
}

#ifndef CONFIG_FPU_ALLOC_TYPED
static Kmem_slab _fpu_state_allocator(
  quota_offset(Fpu::state_size()) + sizeof(Ram_quota *),
  Fpu::state_align(), "Fpu state");

bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s)
{
  if (!_alloc_state(q, s, Fpu::state_size(), _fpu_state_allocator))
    return false;

  Fpu::init_state(s.get());
  return true;
}

void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (!s)
    return;

  _free_state(s, Fpu::state_size(), _fpu_state_allocator);
}
#else

#include <fpu_alloc_typed_slab.h>

bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s, Fpu::State_type type)
{
  if (!_alloc_state(q, s, Fpu::state_size(type), *Fpu_alloc::slab_alloc(type)))
    return false;

  Fpu::init_state(s.get(), type);
  return true;
}

void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (!s)
    return;

  Fpu::State_type type = s.get()->type();
  _free_state(s, Fpu::state_size(type), *Fpu_alloc::slab_alloc(type));
}

#endif
