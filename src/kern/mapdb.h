#pragma once

#include "slab_cache.h"
#include "l4_types.h"
#include "types.h"
#include "mapping.h"
#include "mapping_tree.h"
#include "auto_quota.h"
#include "assert_opt.h"
#include <unique_ptr.h>

#include <cassert>

class Space;

//
// class Physframe
//

/** Array elements for holding frame-specific data. */
class Physframe : public Base_mappable
{
  friend class Mapdb;
  friend class Treemap;
  friend class Jdb_mapdb;

public:
  static constexpr unsigned long mem_size(size_t size)
  { return (size * sizeof(Physframe) + 1023) & ~1023; }

  static constexpr Bytes mem_bytes(size_t size)
  { return Bytes(mem_size(size)); }

private:
  ~Physframe()
  {
    assert (!has_mappings());
  }

  static Physframe *alloc(size_t size)
  {
#if 1				// Optimization: See constructor
    void *mem = Kmem_alloc::allocator()->alloc(mem_bytes(size));
    if (mem)
      memset(mem, 0, size * sizeof(Physframe));
    return (Physframe *)mem;
#else
    Physframe* block
      = (Physframe *)Kmem_alloc::allocator()->alloc(mem_bytes(size));
    assert (block);
    for (unsigned i = 0; i < size; ++i)
      new (block + i) Physframe();

    return block;
#endif
  }

  static void
  free(Physframe *block, size_t size, Space *owner);

  void del(Space *owner);


}; // struct Physframe


//
// class Treemap
//

class Treemap
{
public:
  using Mapping = ::Mapping;
  using Page    = Mapping_tree::Page;
  using Pfn     = Mapping::Pfn;
  using Pcnt    = Mapping::Pcnt;
  using Order   = Mdb_types::Order;

private:
  bool match(Mapping const *m, Space const *spc, Pfn va) const
  {
    return (m->space() == spc)
           && (vaddr(m) == cxx::mask_lsb(va, _page_shift));
  }

  static Bytes quota_size(Page key_end)
  {
    return Bytes(Physframe::mem_size(cxx::int_value<Page>(key_end))
                     + sizeof(Treemap));
  }

  // get the virtual address coresponding to the given physframe
  // inside this map
  Pfn frame_vaddr(Physframe const *f) const
  {
    return  _page_offset + (Pcnt(f - _physframe) << _page_shift);
  }

public:
  class Frame
  {
  public:
    using Iterator = Mapping_tree::Iterator;

    Iterator m;
    Treemap *treemap;
    Physframe *frame = nullptr;

    Pfn vaddr(Mapping_tree::Iterator m) const
    { return treemap->vaddr(*m); }

    Pfn vaddr() const
    { return treemap->vaddr(*m); }

    Order page_shift() const
    { return treemap->page_shift(); }

    bool same_lock(Frame const &o) const
    {
      return frame == o.frame;
    }

    void clear()
    {
      frame->lock.clear();
      frame = nullptr;
    }

    void clear_both(Physframe *f)
    {
      if (f != frame)
        f->lock.clear();

      clear();
    }

    void might_clear()
    {
      if (frame)
        clear();
    }

    void set(Mapping_tree::Iterator ma, Treemap *tm, Physframe *pf)
    {
      m = ma;
      treemap = tm;
      frame = pf;
    }

    Space *pspace() const
    { return *m ? m->space() : treemap->owner(); }

    Pfn pvaddr() const
    { return *m ? vaddr() : treemap->frame_vaddr(frame); }

    int next_depth() const
    { return *m ? m->depth() + 1 : 0; }

    Iterator next_mapping() const
    { return *m ? ++Iterator(m) : frame->first(); }
  };

private:
  friend class Jdb_mapdb;
  friend class Treemap_ops;

  // DATA
  Page _key_end;		///< Number of Physframe entries
  Space *_owner_id;	///< ID of owner of mapping trees
  Pfn _page_offset;	///< Virt. page address in owner's addr space
  Order _page_shift;		///< Page size of mapping trees
  Physframe* _physframe;	///< Pointer to Physframe array
  const size_t* const _sub_shifts; ///< Pointer to array of sub-page sizes
  const unsigned _sub_shifts_num;  ///< Number of valid _sub_shifts entries

public:
  Treemap(Page key_end, Space *owner_id,
          Pfn page_offset, Order page_shift,
          const size_t* sub_shifts, unsigned sub_shifts_num,
          Physframe *physframe)
  : _key_end(key_end),
    _owner_id(owner_id),
    _page_offset(page_offset),
    _page_shift(page_shift),
    _physframe(physframe),
    _sub_shifts(sub_shifts),
    _sub_shifts_num(sub_shifts_num)
  {}

  ~Treemap()
  { Physframe::free(_physframe, cxx::int_value<Page>(_key_end), owner()); }

  Page  end() const         { return _key_end; }
  Pfn   page_offset() const { return _page_offset; }
  Pcnt  page_size() const   { return Pcnt(1) << _page_shift; }
  Order page_shift() const  { return _page_shift; }
  Space *owner() const      { return _owner_id; }

  Page round_to_page(Pcnt v) const
  {
    return Page(cxx::int_value<Pcnt>(
          (v + ((Pcnt(1) << _page_shift) - Pcnt(1)))
          >> _page_shift));
  }

  Page trunc_to_page(Pcnt v) const
  { return Page(cxx::int_value<Pcnt>(v >> _page_shift)); }

  Page trunc_to_page(Pfn v) const
  { return Page(cxx::int_value<Pfn>(v >> _page_shift)); }

  Pfn to_vaddr(Page v) const
  { return Pfn(cxx::int_value<Page>(v << _page_shift)); }

  Pcnt size() const
  { return to_vaddr(_key_end) - Pfn(0); }

  Pfn end_addr() const
  { return _page_offset + size(); }

  Pfn vaddr(Mapping const *m) const
  { return to_vaddr(m->page()); }

  void set_vaddr(Mapping* m, Pfn address) const
  { m->set_page(trunc_to_page(address)); }

  Physframe *frame(Page key) const
  { return &_physframe[cxx::int_value<Page>(key)]; }

  static Slab_cache *allocator();
  static Treemap *create(Order parent_page_shift, Space *owner_id,
                         Pfn page_offset,
                         const size_t* shifts, unsigned shifts_num);
  bool lookup(Pcnt key, Space const *search_space, Pfn search_va,
              Frame *res);

  Mapping *insert(Physframe* frame, Mapping_tree::Iterator const &parent,
                  Space *parent_space, Pfn parent_va,
                  Space *space, Pfn va, Pcnt phys, Pcnt size);

  void flush(Physframe* f, Pcnt offs_begin, Pcnt offs_end);

  void flush(Physframe* f, Mapping_tree::Iterator parent,
             bool me_too,
             Pcnt offs_begin, Pcnt offs_end);

  bool grant(Physframe* f, Mapping_tree::Iterator m, Space *new_space, Pfn va);

  void operator delete (void *block)
  {
    Treemap *t = reinterpret_cast<Treemap*>(block);
    Space *id = t->_owner_id;
    auto end = t->_key_end;
    asm ("" : "=m"(t->_owner_id), "=m"(t->_key_end));
    allocator()->free(block);
    Mapping_tree::quota(id)->free(Treemap::quota_size(end));
  }

  Physframe *tree(Page key)
  {
    assert (key < _key_end);

    Physframe *f = frame(key);
    f->lock.lock();
    return f;
  }

  Mapping_tree::Iterator
  _lookup(Physframe *f, Mapping_tree::Iterator m,
          Space const *spc, Pcnt key, Pfn va,
          Frame *res, int *root_depth = 0, int *current_depth = 0)
  {
    auto subkey = cxx::get_lsb(key, _page_shift);
    for (; *m; ++m)
      {
        if (Treemap *sub = m->submap())
          {
            Physframe *f = sub->tree(sub->trunc_to_page(subkey));

            // special sigma0 case for synthetic 1. level mapping nodes
            if (spc->is_sigma0())
              {
                res->set(f->insertion_head(), sub, f);
                return m;
              }

            // XXX Recursion.  The max. recursion depth should better be
            // limited!
            auto ms = sub->_lookup(f, f->tree()->begin(), spc, subkey, va, res,
                                   root_depth, current_depth);

            if (*ms)
              {
                if (ms->submap())
                  f->lock.clear();

                return m;
              }

            f->lock.clear();
            continue;
          }

        if (current_depth)
          {
            *current_depth = m->depth();
            if (*root_depth >= *current_depth)
              *root_depth = -3;
          }

        if (match(*m, spc, va))
          {
            res->set(m, this, f);
            return m;
          }
      }
    return m;
  }

  int lookup_src_dst(Space const *src, Pcnt src_key, Pfn src_va,
                     Space const *dst, Pcnt dst_key, Pfn dst_va,
                     Frame *src_frame, Frame *dst_frame)
  {
    assert_opt (!src_frame->frame);
    assert_opt (!dst_frame->frame);

    Order const ps = _page_shift;
    auto const src_k = trunc_to_page(src_key);
    auto const dst_k = trunc_to_page(dst_key);

    assert (src_k < _key_end);
    assert (dst_k < _key_end);

    if (src_k != dst_k)
      {
        // different phys frames, do separate lookups
        if (!lookup(dst_key, dst, dst_va, dst_frame))
          return -1;

        if (!lookup(src_key, src, src_va, src_frame))
          {
            // src mapping not found -> we cannot map
            // free the dst frame and abort
            dst_frame->clear();
            return -1;
          }

        // src and dst found but no upgrade possible
        return 1; // --> unmap dst then map
      }

    // we give prio to the dst mapping
    Physframe *f = tree(dst_k);
    auto t = f->tree();

    int c_depth = f->min_depth() - 1; // depth of current node
    Mapping_tree::Iterator m = t->begin();

    // special sigma0 case for synthetic 1. level mapping nodes
    if (*m && src->is_sigma0())
      {
        assert (src == _owner_id);
        src_frame->set(f->insertion_head(), this, f);
      }

    for (; *m && !src_frame->frame && !dst_frame->frame; ++m)
      {
        if (Treemap *sub = m->submap())
          {
            auto const dst_subkey = cxx::get_lsb(dst_key, ps);
            auto const src_subkey = cxx::get_lsb(src_key, ps);
            int r = sub->lookup_src_dst(src, src_subkey, src_va,
                                        dst, dst_subkey, dst_va,
                                        src_frame, dst_frame);
            if (r >= 0)
              {
                f->lock.clear();
                return r;
              }

            continue;
          }

        c_depth = m->depth();

        if (match(*m, src, src_va))
          src_frame->set(m, this, f);

        if (match(*m, dst, dst_va))
          dst_frame->set(m, this, f);
      }

    if (!src_frame->frame && !dst_frame->frame)
      {
        f->lock.clear();
        return -1; // nothing found
      }
    else if (src_frame->frame && dst_frame->frame)
      // src == dst
      return 2; // unmap, but no map
    else if (src_frame->frame)
      {
        // look for dst only
        int r_depth = c_depth;
        m = _lookup(f, m, dst, dst_key, dst_va, dst_frame,
                    &r_depth, &c_depth);
        if (!*m)
          {
            src_frame->clear_both(f);
            return -1; // nothing found
          }

        if (!m->submap())
          {
            if (r_depth + 1 == c_depth)
              return 0; // upgrade

            return 1;
          }

        if (r_depth == c_depth && dst_frame->m->depth() == f->min_depth())
          return 0;

        // found dst and src, do not unlock because src_frame
        // needs to be kept locked
        return 1;
      }
    else
      {
        // src not yet found, look for src only
        int r_depth = c_depth;
        m = _lookup(f, m, src, src_key, src_va, src_frame,
                    &r_depth, &c_depth);

        if (!*m)
          {
            // nothing found
            dst_frame->clear_both(f);
            return -1;
          }

        if (!m->submap())
          {
            // same mapping size
            if (r_depth < (int)f->min_depth() - 1)
              return 1; // in another subtree -> unmap + map

            return 2; // in the same subtree -> unmap + no map
          }

        // smaller mapping in submap found...
        if (r_depth == c_depth)
          {
            // src is in subtree of dst -> unmap dst and noting to map then
            src_frame->clear();
            return 2; // unmap + no map
          }
        // src is in a sibling subtree -> unmap + map
        // needs to be kept locked
        return 1;
      }
  }

  template<typename F>
  void for_range(Page start, Page end, F &&func)
  {
    Order ps = page_shift();

    for (Page sub_page = start; sub_page < end; ++sub_page)
      {
        Physframe *f = frame(sub_page);
        if (!f->has_mappings())
          continue;

        auto guard = lock_guard(f->lock);
        func(f, ps);
      }
  }
};

/** A mapping database.
 */
class Mapdb
{
  friend class Jdb_mapdb;

public:
  using Mapping = ::Mapping;
  using Pfn =     Treemap::Pfn;
  using Pcnt =    Treemap::Pcnt;
  using Order =   Treemap::Order;
  using Frame =   Treemap::Frame;

  Treemap *dbg_treemap() const
  { return _treemap.get(); }

  bool valid_address(Pfn phys) const
  {
    // on the root level physical and virtual frame numbers
    // are the same
    return phys < _treemap->end_addr();
  }

private:
  // DATA
  cxx::unique_ptr<Treemap> const _treemap;

  template<typename F> static
  void _for_full_subtree(unsigned min_depth,
                         Mapping_tree::Iterator first,
                         Mapdb::Order size,
                         F &&func)
  {
    typedef Mapping::Page Page;
    for (auto cursor = first; *cursor; ++cursor)
      {
        if (Treemap *map = cursor->submap())
          map->for_range(Page(0), map->end(), [func](Physframe *f, Mapdb::Order size)
              {
                _for_full_subtree(f->min_depth(), f->first(), size,
                                  func);

              });
        else if (cursor->depth() >= min_depth)
          func(*cursor, size);
        else
          break;
      }
  }

  template<typename F> static
  void _foreach_mapping(unsigned min_depth,
                        Mapping_tree::Iterator cursor,
                        Mapdb::Order size,
                        Mapdb::Pfn va_begin, Mapdb::Pfn va_end,
                        F &&func)
  {
    typedef Mapping::Page Page;

    if (!*cursor)
      return;

    if (Treemap *submap = cursor->submap())
      {
        Order ps = submap->page_shift();
        Pfn po = submap->page_offset();
        Page start(0);
        Page end(submap->end());

        if (va_begin > po)
          start = Page(cxx::int_value<Pcnt>((va_begin - po) >> ps));

        if (va_end < po + submap->size())
          end = submap->round_to_page(va_end - po);

        submap->for_range(start, end, [&func, va_begin, va_end](Physframe *f, Mapdb::Order size)
            {
              _foreach_mapping(f->min_depth(), f->first(),
                               size, va_begin, va_end, func);
            });

        ++cursor;
      }

    _for_full_subtree(min_depth, cursor, size, cxx::forward<F>(func));
  }

public:
  template<typename F> static
  void foreach_mapping(Frame const &f,
                       Mapdb::Pfn va_begin, Mapdb::Pfn va_end,
                       F &&func)
  {
    _foreach_mapping(f.next_depth(), f.next_mapping(),
                     f.treemap->page_shift(),
                     va_begin, va_end, cxx::forward<F>(func));
  }

  Mapdb(Space *owner, Order parent_page_shift, size_t const *page_shifts,
        unsigned page_shifts_num)
  : _treemap(Treemap::create(parent_page_shift, owner,
                             Pfn(0), page_shifts, page_shifts_num))
  {
    // assert (boot_time);
    assert (_treemap);
  }

  /** Insert a new mapping entry with the given values as child of
      "parent".
      We assume that there is at least one free entry at the end of the
      array so that at least one insert() operation can succeed between a
      lookup()/free() pair of calls.  This is guaranteed by the free()
      operation which allocates a larger tree if the current one becomes
      to small.
      @param parent Parent mapping of the new mapping.
      @param space  Number of the address space into which the mapping is entered
      @param va     Virtual address of the mapped page.
      @param size   Size of the mapping.  For memory mappings, 4K or 4M.
      @return If successful, new mapping.  If out of memory or mapping
             tree full, 0.
      @post  All Mapping* pointers pointing into this mapping tree,
             except "parent" and its parents, will be invalidated.
   */
  Mapping *
  insert(Frame const &frame, Space *space,
         Pfn va, Pfn phys, Pcnt size)
  {
    return frame.treemap->insert(frame.frame, frame.m,
                                 frame.pspace(), frame.pvaddr(),
                                 space, va, phys - Pfn(0), size);
  } // insert()


  /**
   * Lookup a mapping and lock the corresponding mapping tree.  The returned
   * mapping pointer, and all other mapping pointers derived from it, remain
   * valid until free() is called on one of them.  We guarantee that at most
   * one insert() operation succeeds between one lookup()/free() pair of calls
   * (it succeeds unless the mapping tree is full).
   * @param space Number of virtual address space in which the mapping
   *              was entered
   * @param va    Virtual address of the mapping
   * @param phys  Physical address of the mapped page frame
   * @return mapping, if found; otherwise, 0
   */
  bool lookup(Space const *space, Pfn va, Pfn phys,
              Frame *res)
  {
    return _treemap->lookup(phys - Pfn(0), space, va, res);
  }

  /** Delete mappings from a tree.  This function deletes mappings
      recursively.
      @param m Mapping that denotes the subtree that should be deleted.
      @param me_too If true, delete m as well; otherwise, delete only
             submappings.
   */
  static
  void flush(Frame const &f, L4_map_mask mask,
             Pfn va_start, Pfn va_end)
  {
    Pcnt size = f.treemap->page_size();
    Pcnt offs_begin = va_start > f.pvaddr()
                    ? va_start - f.pvaddr()
                    : Pcnt(0);
    Pcnt offs_end = va_end > f.pvaddr() + size
                  ? size
                  : va_end - f.pvaddr();

    f.treemap->flush(f.frame, f.m, mask.self_unmap(), offs_begin, offs_end);
  } // flush()

  /** Change ownership of a mapping.
      @param m Mapping to be modified.
      @param new_space Number of address space the mapping should be
                       transferred to
      @param va Virtual address of the mapping in the new address space
   */
  bool grant(Frame const &f, Space *new_space,
             Pfn va)
  {
    return f.treemap->grant(f.frame, f.m, new_space, va);
  }

  int lookup_src_dst(Space const *src, Pfn src_phys, Pfn src_va,
                     Space const *dst, Pfn dst_phys, Pfn dst_va,
                     Frame *src_frame, Frame *dst_frame)
  {
    return _treemap->lookup_src_dst(src, src_phys - Pfn(0), src_va,
                                    dst, dst_phys - Pfn(0), dst_va,
                                    src_frame, dst_frame);
  }

};


inline
void
Physframe::del(Space *owner)
{
  if (has_mappings())
    {
      auto guard = lock_guard(lock);

      // Find next-level trees.
      for (auto m = tree()->begin(); *m; ++m)
        {
          if (m->submap())
            delete m->submap();
        }

      erase_tree(owner);
    }
}

