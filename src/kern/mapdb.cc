#include "mapdb.h"

#include <cassert>
#include <cstring>

#include "assert_opt.h"
#include "config.h"
#include "globals.h"
#include "helping_lock.h"
#include "kmem_alloc.h"
#include "kmem_slab.h"
#include "ram_quota.h"
#include "std_macros.h"
#include <new>


static Kmem_slab_t<Treemap> _treemap_allocator("Treemap");

void
Physframe::free(Physframe *block, size_t size, Space *owner)
{
  for (unsigned i = 0; i < size; ++i)
    {
      block[i].del(owner);
      block[i].~Physframe();
    }

  Kmem_alloc::allocator()->free(Physframe::mem_bytes(size), block);
}


//
// class Treemap_ops
//
// Helper operations for Treemaps (used in Mapping_tree::flush)

class Treemap_ops
{
private:
  using Order = Treemap::Order;
  using Page  = Treemap::Page;
  using Pfn   = Treemap::Pfn;
  using Pcnt  = Treemap::Pcnt;
  Order _page_shift;

public:
  explicit Treemap_ops(Order page_shift)
  : _page_shift(page_shift)
  {}

  Pfn to_vaddr(Page v) const
  { return Pfn(cxx::int_value<Page>(v << _page_shift)); }

  Pcnt page_size() const
  { return Pcnt(1) << _page_shift; }

  Space *owner(Treemap const *submap) const
  { return submap->owner(); }

  bool is_partial(Treemap const *submap, Pcnt offs_begin,
                  Pcnt offs_end) const
  {
    return offs_begin > Pcnt(0)
           || offs_end < submap->size();
  }

  void del(Treemap *submap) const
  { delete submap; }

  unsigned long mem_size(Treemap const *submap) const
  {
    unsigned long quota
      = cxx::int_value<Page>(submap->_key_end) * sizeof(Physframe) + sizeof(Treemap);

    for (Page key = Page(0);
        key < submap->_key_end;
        ++key)
      {
        Physframe *f = submap->frame(key);
        quota += f->base_quota_size();
        if (!f->has_mappings())
          continue;

        // recurse down through all page sizes
        if (Treemap *s = f->find_submap(f->insertion_head()))
          quota += mem_size(s);
      }

    return quota;
  }

  void grant(Treemap *submap, Space *old_space,
             Space *new_space, Page virt_page) const
  {
    submap->_owner_id = new_space;
    submap->_page_offset = to_vaddr(virt_page);
    auto const end = submap->_key_end;
    auto const subva = virt_page << (_page_shift - submap->_page_shift);

    for (Page key = Page(0); key < end; ++key)
      {
        auto f = submap->frame(key);
        if (!f->has_mappings())
          continue;

        // recurse down through all page sizes
        if (Treemap *s = f->find_submap(f->insertion_head()))
          Treemap_ops(submap->_page_shift).grant(s, old_space, new_space,
                      subva + key);
      }
  }


  void flush(Treemap *submap,
             Treemap::Pcnt offs_begin, Treemap::Pcnt offs_end) const
  {
    for (Treemap::Page i = submap->trunc_to_page(offs_begin);
        i < submap->round_to_page(offs_end);
        ++i)
      {
        Pcnt page_offs_begin = submap->to_vaddr(i) - Pfn(0);
        Pcnt page_offs_end = page_offs_begin + submap->page_size();

        Physframe* subframe = submap->frame(i);

        if (!subframe->has_mappings())
          continue;

        auto guard = lock_guard(subframe->lock);

        if (offs_begin <= page_offs_begin && offs_end >= page_offs_end)
          subframe->erase_tree(submap->owner());
        else
          {
            submap->flush(subframe,
                          page_offs_begin > offs_begin
                            ? Pcnt(0)
                            : cxx::get_lsb(offs_begin - page_offs_begin, _page_shift),
                          page_offs_end < offs_end
                            ? page_size()
                            : cxx::get_lsb(offs_end - page_offs_begin - Pcnt(1), _page_shift) + Pcnt(1));
          }
      }
  }
};


//
// Treemap members
//

Treemap *
Treemap::create(Order parent_page_shift, Space *owner_id,
                Pfn page_offset,
                const size_t* shifts, unsigned shifts_num)
{
  Order page_shift(shifts[0]);
  Page key_end = Page(1) << (parent_page_shift - page_shift);
  Auto_quota<Ram_quota> quota(Mapping_tree::quota(owner_id), quota_size(key_end));

  if (EXPECT_FALSE(!quota))
    return 0;

  Physframe *pf = Physframe::alloc(cxx::int_value<Page>(key_end));

  if (EXPECT_FALSE(!pf))
    return 0;

  void *m = allocator()->alloc();
  if (EXPECT_FALSE(!m))
    {
      Physframe::free(pf, cxx::int_value<Page>(key_end), owner_id);
      return 0;
    }

  quota.release();
  return new (m) Treemap(key_end, owner_id, page_offset, page_shift, shifts + 1,
                         shifts_num - 1, pf);
}


Slab_cache *
Treemap::allocator()
{ return _treemap_allocator.slab(); }


bool
Treemap::lookup(Pcnt key, Space const *search_space, Pfn search_va,
                Frame *res)
{
  // get and lock the tree.
  assert (trunc_to_page(key) < _key_end);
  Physframe *f = tree(trunc_to_page(key)); // returns locked frame

  // special sigma0 case for synthetic 1. level mapping nodes
  if (search_space->is_sigma0())
    {
      assert (_owner_id == search_space);
      res->set(f->insertion_head(), this, f);
      return true;
    }

  auto m = _lookup(f, f->tree()->begin(), search_space, key, search_va, res);
  if (*m)
    {
      if (m->submap())
        f->lock.clear();

      return true;
    }

  // not found, or found in submap -- unlock tree
  f->lock.clear();

  return false;
}

Mapping *
Treemap::insert(Physframe* frame, Mapping_tree::Iterator const &parent,
                Space *parent_space, Pfn parent_va,
                Space *space, Pfn va, Pcnt phys, Pcnt size)
{
  using Iterator = Mapping_tree::Iterator;

  if (page_size() == size)  // normal mapping
    {
      Iterator free = frame->tree()->allocate(Mapping_tree::quota(space),
                                              parent);
      if (EXPECT_FALSE(!*free))
        return 0;

      free->set_space(space);
      set_vaddr(*free, va);
      return *free;
    }

  // Inserting subpage mapping.  See if we can find a submap.  If so,
  // we don't have to allocate a new Mapping entry.
  Treemap* submap = frame->find_submap(parent);

  if (! submap)  // Need allocation of new entry for submap
    {
      // first check quota! In case of a new submap the parent pays for
      // the node...
      Ram_quota *payer = Mapping_tree::quota(parent_space);

      Iterator free = frame->tree()->allocate_submap(payer, parent);
      if (EXPECT_FALSE(!*free))
        return 0;

      assert (_sub_shifts_num > 0);

      submap = Treemap::create(_page_shift, parent_space, parent_va,
                               _sub_shifts, _sub_shifts_num);
      if (! submap)
        {
          // free the mapping got with allocate
          frame->free_mapping(payer, free);
          return 0;
        }

      free->set_submap(submap);
    }

  Pcnt subframe_offset = cxx::mask_lsb(cxx::get_lsb(phys, _page_shift),
                                       submap->page_shift());
  Physframe* subframe = submap->tree(submap->trunc_to_page(subframe_offset));
  if (! subframe)
    return 0;

  Mapping* ret = submap->insert(subframe, subframe->insertion_head(),
                                parent_space, parent_va + subframe_offset,
                                space, va, phys, size);

  subframe->release();

  return ret;
} // Treemap::insert()

void
Treemap::flush(Physframe* f, Pcnt offs_begin, Pcnt offs_end)
{
  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  f->flush(offs_begin, offs_end, Treemap_ops(_page_shift));
  return;
} // Treemap::flush()

void
Treemap::flush(Physframe* f, Mapping_tree::Iterator parent,
               bool me_too,
               Pcnt offs_begin, Pcnt offs_end)
{
  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  f->flush(parent, me_too, offs_begin, offs_end, Treemap_ops(_page_shift));
  return;
} // Treemap::flush()

bool
Treemap::grant(Physframe* f, Mapping_tree::Iterator m, Space *new_space, Pfn va)
{
  return f->grant(m, new_space, trunc_to_page(va), Treemap_ops(_page_shift));
}

