#pragma once

#include <auto_quota.h>
#include <cxx/slist>

#include <spin_lock.h>
#include <lock_guard.h>
#include <mem_layout.h>
#include <kmem_alloc_arch.h>
#include <initcalls.h>

#include <cassert>

class Buddy_alloc;
class Mem_region_map_base;
class Kip;
class Mem_desc;

template<typename Q> class Kmem_q_alloc;

class Kmem_alloc
{
  friend class Kmem_alloc_tester;

public:
  typedef Buddy_alloc Alloc;

  Kmem_alloc() FIASCO_INIT;

  static Address to_phys(void *v)
  {
    return Mem_layout::pmem_to_phys(v);
  }

  static void init();

  static Kmem_alloc *allocator()
  {
    assert (_alloc /* uninitialized use of Kmem_alloc */);
    return _alloc;
  }

  template<typename Q> static
  Kmem_q_alloc<Q> q_allocator(Q *quota);


  void *alloc(Order o)
  {
    return alloc(Bytes(1) << o);
  }

  void free(Order o, void *p)
  {
    free(Bytes(1) << o, p);
  }

  template<typename T>
  T *alloc_array(unsigned elems)
  {
    return new (this->alloc(Bytes(sizeof(T) * elems))) T[elems];
  }

  template<typename T>
  void free_array(T *b, unsigned elems)
  {
    for (unsigned i = 0; i < elems; ++i)
      b[i].~T();
    this->free(Bytes(sizeof(T) * elems), b);
  }

  void *alloc(Bytes size);
  void free(Bytes size, void *page);
  void dump() const;
  void debug_dump() const;

  template<typename T = void, typename Q = void>
  T *q_alloc(Q *quota, Order order)
  {
    Auto_quota<Q> q(quota, order);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b = alloc(order);
    if (EXPECT_FALSE(!b))
      return 0;

    q.release();
    return reinterpret_cast<T*>(b);
  }

  template<typename T = void, typename Q = void>
  T *q_alloc(Q *quota, Bytes size = Bytes(sizeof(T)))
  {
    Auto_quota<Q> q(quota, size);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b;
    if (EXPECT_FALSE(!(b = alloc(size))))
      return 0;

    q.release();
    return reinterpret_cast<T*>(b);
  }

  void free_phys(Order o, Address p)
  {
    void *va = reinterpret_cast<void*>(Mem_layout::phys_to_pmem(p));
    if ((unsigned long)va != ~0UL)
      free(o, va);
  }

  template< typename Q >
  void q_free_phys(Q *quota, Order order, Address obj)
  {
    free_phys(order, obj);
    quota->free(Bytes(1) << order);
  }

  template< typename Q >
  void q_free(Q *quota, Order order, void *obj)
  {
    free(order, obj);
    quota->free(Bytes(1) << order);
  }

  template< typename Q >
  void q_free(Q *quota, Bytes size, void *obj)
  {
    free(size, obj);
    quota->free(size);
  }

  static unsigned long
  create_free_map(Kip const *kip, Mem_region_map_base *map);

  static FIASCO_INIT
  unsigned long determine_kmem_alloc_size(unsigned long available_size,
                                          unsigned long alignment = Config::PAGE_SIZE);

  static bool ready() { return _alloc != nullptr; }

protected:
  static void allocator(Kmem_alloc *a)
  {
    _alloc = a;
  }

private:
  typedef Spin_lock<> Lock;
  static Lock lock;
  static Alloc *a;
  static unsigned long _orig_free;
  static Kmem_alloc *_alloc;

  unsigned long orig_free() const
  {
    return _orig_free;
  }

  /**
   * Add a "Kernel_tmp" KIP memory region marked to the kernel memory except a
   * "remaining" part of size `skip` which shall be not considered, change the
   * descriptor type to "Reserved" and update _orig_free.
   */
  static FIASCO_INIT
  void add_kip_md_tmp_to_kmem_sans_size(Mem_desc *md, unsigned long skip);

  /**
   * Add all "Kernel_tmp" KIP memory regions completely to the kernel memory,
   * change the descriptor types to "Reserved" and update _orig_free.
   */
  static FIASCO_INIT
  void add_kip_md_tmp_to_kmem();

  static FIASCO_INIT
  void setup_kmem_from_kip_md_tmp(unsigned long freemap_size,
                                  unsigned long min_addr_kern);
};


class Kmem_alloc_reaper : public cxx::S_list_item
{
  size_t (*_reap)(bool desperate);

public:
  Kmem_alloc_reaper(size_t (*reap)(bool desperate))
  : _reap(reap)
  {
    mem_reapers.atomic_add(this);
  }

  static size_t morecore(bool desperate = false)
  {
    size_t freed = 0;

    for (Reaper_list::Const_iterator reaper = mem_reapers.begin();
         reaper != mem_reapers.end(); ++reaper)
      freed += reaper->_reap(desperate);

    return freed;
  }

private:
  typedef cxx::S_list_bss<Kmem_alloc_reaper> Reaper_list;
  static Reaper_list mem_reapers;
};

template<typename Q>
class Kmem_q_alloc
{
public:
  Kmem_q_alloc(Q *q, Kmem_alloc *a) : _a(a), _q(q) {}
  bool valid() const { return _a && _q; }
  void *alloc(Bytes size) const
  {
    Auto_quota<Q> q(_q, size);
    if (EXPECT_FALSE(!q))
      return 0;

    void *b;
    if (EXPECT_FALSE(!(b = _a->alloc(size))))
      return 0;

    q.release();
    return b;
  }

  void free(void *block, Bytes size) const
  {
    _a->free(size, block);
    _q->free(size);
  }

  template<typename V>
  Phys_mem_addr::Value to_phys(V v) const
  { return _a->to_phys(v); }

private:
  Kmem_alloc *_a;
  Q *_q;
};

template<typename Q> inline
Kmem_q_alloc<Q>
Kmem_alloc::q_allocator(Q *quota)
{
  assert (_alloc /* uninitialized use of Kmem_alloc */);
  return Kmem_q_alloc<Q>(quota, _alloc);
}

