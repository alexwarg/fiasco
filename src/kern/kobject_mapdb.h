#pragma once

#include "types.h"
#include "space.h"
#include "kobject.h"

#include "config.h"
#include "globals.h"
#include "std_macros.h"

#include <cassert>

class Kobject;

/** A mapping database.
 */
class Kobject_mapdb
{
public:
  // TYPES

  typedef Obj_space::Phys_addr Phys_addr;
  typedef Obj_space::V_pfn Vaddr;
  typedef Obj::Mapping Mapping;
  typedef int Order;

  class Iterator;
  class Frame
  {
  public:
    // initializing frame is not needed but GCC complains with
    // "may be used uninitialized" in map_util-objs map ...
    // triggering a warning in Kobject_mapdb::grant
    //
    // As common perception seems to be that compiling without warnings is
    // more important than runtime we always initialize frame to 0 in the
    // constructor, even if this would probably cause more harm than good if
    // used with a 0 pointer as there could be a page mapped at 0 as well
    Kobject_mappable* frame = nullptr;
    Mapping *m = nullptr;

    void clear()
    {
      frame->_lock.clear();
      frame = nullptr;
    }

    void might_clear()
    {
      if (frame)
        clear();
    }

    bool same_lock(Frame const &o) const
    {
      return frame == o.frame;
    }
  };

  template< typename F >
  static void foreach_mapping(Frame const &, Obj_space::V_pfn, Obj_space::V_pfn, F)
  {}

  inline static
  bool lookup(Space const *, Vaddr va, Phys_addr obj,
              Frame *out)
  {
    Kobject_mappable *rn = obj->map_root(); 
    rn->_lock.lock();
    if (va._c->obj() == obj)
      {
        out->m = va._c;
        out->frame = rn;
        return true;
      }

    rn->_lock.clear();
    return false;
  }

  inline static
  int lookup_src_dst(Space const *, Phys_addr sobj, Vaddr sva,
                     Space const *, Phys_addr dobj, Vaddr dva,
                     Frame *sframe, Frame *dframe)
  {
    Kobject_mappable *srn = sobj->map_root();
    Kobject_mappable *drn = dobj->map_root();
    bool same_obj = drn == srn;

    if (same_obj)
      srn->_lock.lock();
    else if (sobj > dobj)
      {
        srn->_lock.lock();
        drn->_lock.lock();
      }
    else
      {
        drn->_lock.lock();
        srn->_lock.lock();
      }

    if (sva._c->obj() != sobj)
      {
        if (!same_obj)
          drn->_lock.clear();
        srn->_lock.clear();
        return -1;
      }

    if (dva._c->obj() != dobj)
      {
        if (!same_obj)
          drn->_lock.clear();
        srn->_lock.clear();
        return -1;
      }

    dframe->m = dva._c;
    dframe->frame = drn;

    if (same_obj && dva._c->delete_rights())
      return 2;

    sframe->m = sva._c;
    sframe->frame = srn;

    return 1;
  }

  static inline
  bool valid_address(Phys_addr obj)
  { return obj; }


  inline static
  Mapping * insert(Frame const &, Space *,
                   Vaddr va, Obj_space::Phys_addr o, Obj_space::V_pfc size)
  {
    (void)size;
    (void)o;
    assert (size == Obj_space::V_pfc(1));

    Mapping *m = va._c;
    Kobject_mappable *rn = o->map_root();
    //LOG_MSG_3VAL(current(), "ins", o->dbg_id(), (Mword)m, (Mword)va._a.value());
    rn->_root.add(m);

    Obj::Entry *e = static_cast<Obj::Entry*>(m);
    if (e->is_ref_counted())
      {
        // No overflow check required. The counter has type Smword and can count
        // half of the addresses in the virtual address space. A capability has a
        // size of at least one Mword but in fact a capability mapping occupies
        // more memory than a single Mword.
        static_assert(sizeof(rn->_cnt) >= sizeof(void*),
                      "Wrong type for reference counter");
        ++rn->_cnt;
      }

    return m;
  }

  inline static
  bool grant(Frame &f, Space *, Vaddr va)
  {
    Obj::Entry *re = va._c;
    Obj::Entry *se = static_cast<Obj::Entry*>(f.m);
    //LOG_MSG_3VAL(current(), "gra", f.frame->dbg_id(), (Mword)sm, (Mword)va._a.value());

    // replace the source cap with the destination cap in the list
    Mapping::List::replace(se, re);

    if (se->is_ref_counted() && !re->is_ref_counted())
      if (--f.frame->_cnt <= 0)
        f.frame->invalidate_mappings();

    return true;
  }

  static inline
  void flush(Frame const &f, L4_map_mask mask,
             Obj_space::V_pfn, Obj_space::V_pfn)
  {
    //LOG_MSG_3VAL(current(), "unm", f.frame->dbg_id(), (Mword)m, 0);
    if (!mask.self_unmap())
      return;

    if (mask.do_delete() && f.m->delete_rights())
      {
        f.frame->invalidate_mappings();
        return;
      }

    if (!static_cast<Obj::Entry*>(f.m)->is_ref_counted())
      {
        Mapping::List::remove(f.m);
        return;
      }

    if (f.frame->_cnt <= 1)
      {
        f.frame->invalidate_mappings();
        return;
      }

    --f.frame->_cnt;
    Mapping::List::remove(f.m);

  } // flush()
};
