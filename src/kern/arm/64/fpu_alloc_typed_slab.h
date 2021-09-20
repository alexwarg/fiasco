
#pragma once

#include <ram_quota.h>
#include <kmem_slab.h>
#include <cpu.h>
#include <fpu.h>

static Kmem_slab _fpu_state_allocator(
    quota_offset(Fpu::state_size(Fpu::State_type::Simd)) + sizeof(Ram_quota *),
    Fpu::state_align(), "Fpu state");

static Kmem_slab  _sve_state_allocator(
    quota_offset(Fpu::state_size(Fpu::State_type::Sve)) + sizeof(Ram_quota *),
    Fpu::state_align(), "Sve state");

namespace Fpu_alloc {

Slab_cache *slab_alloc(Fpu::State_type type)
{
  return type == Fpu::State_type::Sve ? &_sve_state_allocator
                                      : &_fpu_state_allocator;
}

}

