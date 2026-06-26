#pragma once

#include <spin_lock.h>
#include <cxx/hlist>
#include <cxx/slist>
#include <auto_quota.h>

#include <cassert>

// The anonymous slab allocator.  You can specialize this allocator by
// providing your own initialization functions and your own low-level
// allocation functions.

class Slab : public cxx::H_list_item
{
private:
  typedef cxx::S_list_item Slab_entry;
  typedef cxx::S_list<Slab_entry> Free_list;

  Free_list _free;
  unsigned short _elems;
  unsigned short _in_use;

  // default deallocator must not be called -- must use explicit destruction
  void operator delete(void* /*block*/)
  {
    assert (!"slab::operator delete called");
  }

public:
  Slab(const Slab&) = delete;

  enum
  {
    Min_obj_align = __alignof(cxx::S_list_item),
    Min_obj_size  = sizeof(cxx::S_list_item),
  };

  Slab(unsigned elems, unsigned entry_size, void *mem)
  : _elems(elems), _in_use(0)
  {
    // Compute pointer to first data element, now taking into account
    // the latest colorization offset
    char *data = static_cast<char*>(mem);

    // Initialize the cache elements
    for (unsigned i = elems; i > 0; --i)
      {
        Slab_entry *e = reinterpret_cast<Slab_entry *>(data);
        _free.push_front(e);
        data += entry_size;
      }
  }

  void *alloc()
  {
    Slab_entry *e = _free.pop_front();

    if (! e)
      return nullptr;

    ++_in_use;
    return e;
  }

  void free(void *entry)
  {
    _free.add(static_cast<Slab_entry *>(entry));

    assert(_in_use);
    --_in_use;
  }

  bool is_empty() const
  {
    return _in_use == 0;
  }

  bool is_full() const
  {
    return _in_use == _elems;
  }

  unsigned in_use() const
  {
    return _in_use;
  }

  void *operator new(size_t, void *block) noexcept
  {
    // slabs must be size-aligned so that we can compute their addresses
    // from element addresses
    return block;
  }
};

class Slab_cache
{
public:
  enum
  {
    Min_obj_align = Slab::Min_obj_align,
    Min_obj_size  = Slab::Min_obj_size,
  };

  static constexpr unsigned entry_size(unsigned elem_size, unsigned alignment)
  { return (elem_size + alignment - 1) & ~(alignment - 1); }

  Slab_cache() = delete;
  Slab_cache(const Slab_cache&) = delete;

  Slab_cache(unsigned elem_size, unsigned alignment,
             char const * name, unsigned long min_size,
             unsigned long max_size)
  : _entry_size(entry_size(elem_size, alignment)), _num_empty(0),
    _name (name)
  {
    lock.init();

    for (_slab_size = min_size;
         (_slab_size - sizeof(Slab)) / _entry_size < 8 && _slab_size < max_size;
         _slab_size <<= 1)
      ;

    _elem_num = (_slab_size - sizeof(Slab)) / _entry_size;
  }

  Slab_cache(unsigned long slab_size, unsigned elem_size,
             unsigned alignment, char const * name)
  : _slab_size(slab_size), _entry_size(entry_size(elem_size, alignment)),
    _num_empty(0), _name (name)
  {
    lock.init();
    _elem_num = (_slab_size - sizeof(Slab)) / _entry_size;
  }

  virtual ~Slab_cache() = default;

  void *alloc();	// request initialized member from cache
  void free(void *cache_entry); // return initialized member to cache

  template< typename Q > inline
  void *q_alloc(Q *quota)
  {
    Auto_quota<Q> q(quota, _entry_size);
    if (EXPECT_FALSE(!q))
      return nullptr;

    void *r;
    if (EXPECT_FALSE(!(r=alloc())))
      return nullptr;

    q.release();
    return r;
  }

  template< typename Q > inline
  void q_free(Q *quota, void *obj)
  {
    free(obj);
    quota->free(_entry_size);
  }

  unsigned long reap();		// request that cache returns memory to system
  void debug_dump() const;

protected:
  void destroy()	// descendant should call this in destructor
  {}

protected:
  friend class Slab;
  friend class Slab_cache_tester;

  // Low-level allocator functions:

  // Allocate/free a block.  "size" is always a multiple of PAGE_SIZE.
  virtual void *block_alloc(unsigned long size, unsigned long alignment) = 0;
  virtual void block_free(void *block, unsigned long size) = 0;

private:
  Slab *get_available_locked()
  {
    Slab *s = _partial.front();
    if (s)
      return s;

    s = _empty.front();
    if (s)
      {
        --_num_empty;
        _empty.remove(s);
        _partial.add(s);
      }

    return s;
  }

  //
  // data declaration follows
  // 

  typedef cxx::H_list<Slab> Slab_list;
  Slab_list _full;    ///< List of full slabs
  Slab_list _partial; ///< List of partially filled slabs
  Slab_list _empty;   ///< List of empty slabs


  unsigned long _slab_size;
  unsigned _entry_size, _elem_num;
  unsigned _num_empty;
  typedef Spin_lock<> Lock;
  Lock lock;
  char const *_name;
};


