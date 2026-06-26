#pragma once

#include "buddy_alloc.h"
#include "kmem_alloc.h"
#include "config.h"
#include "lock_guard.h"
#include "spin_lock.h"

#include "slab_cache.h"		// Slab_cache
#include "minmax.h"

#include <cstddef>		// size_t
#include <cxx/slist>
#include <cxx/type_traits>

class Kmem_slab : public Slab_cache, public cxx::S_list_item
{
  friend class Jdb_kern_info_memory;

  void *block_alloc(unsigned long size, unsigned long) override;
  void block_free(void *block, unsigned long size) override;

public:
  Kmem_slab(unsigned elem_size, unsigned alignment,
            char const *name,
            unsigned long min_size = Buddy_alloc::Min_size,
            unsigned long max_size = Buddy_alloc::Max_size);

  ~Kmem_slab()
  {
    destroy();
  }

  static size_t reap_all(bool desperate);

protected:
  Kmem_slab(unsigned long slab_size, unsigned elem_size,
            unsigned alignment, char const *name);
};


/**
 * Slab allocator for the given size and alignment.
 * \tparam SIZE   Size of an object in bytes.
 * \tparam ALIGN  Alignment of an object in bytes. Must be power of 2.
 *
 * This class provides a per-size instance of a slab cache.
 */
template< unsigned SIZE, unsigned ALIGN = 8 >
class Kmem_slab_for_size
{
  static_assert(
      (ALIGN > 0) && !(ALIGN & (ALIGN - 1)),
      "alignment must be power of 2");
  static_assert(
      Slab_cache::entry_size(SIZE, ALIGN) >= Slab_cache::Min_obj_size,
      "objects too small for slab");

public:
  static void *alloc() { return _s.alloc(); }

  template<typename Q> static
  void *q_alloc(Q *q) { return _s.template q_alloc<Q>(q); }

  static void free(void *e) { _s.free(e); }

  template<typename Q> static
  void q_free(Q *q, void *e) { _s.template q_free<Q>(q, e); }

  static Slab_cache *slab() { return &_s; }

protected:
  static Kmem_slab _s;
};

template< unsigned SIZE, unsigned ALIGN >
Kmem_slab Kmem_slab_for_size<SIZE, ALIGN>::_s(SIZE, ALIGN, "fixed size");


/**
 * Allocator for fixed-size objects based on the buddy allocator.
 * \tparam SIZE  The size and the alignment of an object in bytes.
 *
 * Note: This allocator uses the buddy allocator and allocates memory in sizes
 * supported by the buddy allocator.
 *
 * Note: Kmem_buddy_for_size and Kmem_slab_for_size must provide compatible
 * interfaces in order to be used interchangeably.
 */
template<unsigned SIZE>
class Kmem_buddy_for_size
{
public:
  static void *alloc()
  { return Kmem_alloc::allocator()->alloc(Bytes(SIZE)); }

  template<typename Q> static
  void *q_alloc(Q *q)
  { return Kmem_alloc::allocator()->q_alloc(q, Bytes(SIZE)); }

  static void free(void *e)
  { Kmem_alloc::allocator()->free(Bytes(SIZE), e); }

  template<typename Q> static
  void q_free(Q *q, void *e)
  { Kmem_alloc::allocator()->q_free(q, Bytes(SIZE), e); }
};

/**
 * Meta allocator to select between Kmem_slab_for_size and Kmem_buddy_for_size.
 *
 * \tparam BUDDY  If true the allocator will use Kmem_buddy_for_size, if
 *                false Kmem_slab_for_size.
 * \tparam SIZE   Size of an object (in bytes) that shall be allocated.
 * \tparam ALIGN  Alignment for each object in bytes. Must be power of 2.
 */
template<bool BUDDY, unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc : Kmem_slab_for_size<SIZE, ALIGN> {};

/* Specialization using the buddy allocator */
template<unsigned SIZE, unsigned ALIGN>
struct _Kmem_alloc<true, SIZE, ALIGN> : Kmem_buddy_for_size<SIZE>
{
  static_assert(
      (ALIGN > 0) && !(ALIGN & (ALIGN - 1)),
      "alignment must be power of 2");
  static_assert(
      ALIGN <= SIZE,
      "alignment constraint too strict for buddy allocator");
  static_assert(
      SIZE <= Buddy_alloc::Max_size,
      "size cannot be bigger than maximum buddy allocator block size");
};

/**
 * Generic allocator for objects of the given size and alignment.
 * \tparam SIZE   The size of an object in bytes.
 * \tparam ALIGN  The alignment of each allocated object (in bytes).
 *                Must be power of 2.
 *
 * This allocator uses _Kmem_alloc<> to select between a slab allocator or
 * the buddy allocator depending on the given size of the objects.
 * (Currently all objects bigger that 1 KiB (0x400) are allocated using the
 * buddy allocator.)
 */
template<unsigned SIZE, unsigned ALIGN = 8>
struct Kmem_slab_s : _Kmem_alloc<(SIZE >= 0x400), SIZE, ALIGN> {};

/**
 * Allocator for objects of the given type.
 * \tparam T      Type of the object to be allocated.
 * \tparam ALIGN  Alignment of the objects (in bytes, usually alignof(T)).
 *                Must be power of 2.
 */
template< typename T, unsigned ALIGN = __alignof(T) >
struct Kmem_slab_t
: Kmem_slab_s<sizeof(T), max<unsigned>(ALIGN, Slab_cache::Min_obj_align)>
{
  typedef Kmem_slab_s<sizeof(T), max<unsigned>(ALIGN, Slab_cache::Min_obj_align)> Slab;
public:
  explicit Kmem_slab_t(char const *) {}
  Kmem_slab_t() = default;

  template<typename ...ARGS> static
  T *new_obj(ARGS &&...args)
  {
    void *c = Slab::alloc();
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  template<typename Q, typename ...ARGS> static
  T *q_new(Q *q, ARGS &&...args)
  {
    void *c = Slab::template q_alloc<Q>(q);
    if (EXPECT_TRUE(c != 0))
      return new (c) T(cxx::forward<ARGS>(args)...);
    return 0;
  }

  static void del(T *e)
  {
    e->~T();
    Slab::free(e);
  }

  template<typename Q> static
  void q_del(Q *q, T *e)
  {
    e->~T();
    Slab::template q_free<Q>(q, e);
  }
};

