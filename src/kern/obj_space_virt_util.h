
#pragma once

#include "mem.h"
#include "mem_space.h"
#include "ram_quota.h"
#include "obj_space_types.h"

#include "globalconfig.h"

#include <cstring>
#include <cassert>
#include <cxx/atomic>

#include "config.h"
#include "cpu.h"
#include "kmem_alloc.h"
#include "kmem.h"
#include "mem_layout.h"
#include <paging_bits.h>

template<typename SPACE>
class Obj_space_virt
{
public:
  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;

  bool initialize()
  { return true; }

  /// v_lookup
  bool v_lookup(V_pfn const &virt, Phys_addr *phys,
                Page_order *size, Attr *attribs) FIASCO_FLATTEN
  {
    if (size) *size = Page_order(0);
    Entry *cap;

    if (Optimize_local
        && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
      cap = cap_virt(virt);
    else
      cap = get_cap(virt);

    if (EXPECT_FALSE(!cap))
      {
        if (size) *size = Page_order(Obj::Caps_per_page_ld2);
        return false;
      }

    if (Optimize_local)
      {
        Capability c = Mem_layout::read_special_safe((Capability*)cap);

        if (phys) *phys = c.obj();
        if (c.valid() && attribs)
          *attribs = Attr(c.rights());
        return c.valid();
      }
    else
      {
        Obj::set_entry(virt, cap);
        if (phys) *phys = cap->obj();
        if (cap->valid() && attribs)
          *attribs = Attr(cap->rights());
        return cap->valid();
      }
  }

  /// v_delete
  L4_fpage::Rights v_delete(V_pfn virt, Page_order size,
                            L4_fpage::Rights page_attribs) FIASCO_FLATTEN
  {
    (void)size;
    assert (size == Page_order(0));

    Entry *c;
    if (Optimize_local
        && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
      {
        c = cap_virt(virt);
        if (!c)
          return L4_fpage::Rights(0);

        Capability cap = Mem_layout::read_special_safe((Capability*)c);
        if (!cap.valid())
          return L4_fpage::Rights(0);
      }
    else
      c = get_cap(virt);

    if (c && c->valid())
      {
        if (page_attribs & L4_fpage::Rights::R())
          c->invalidate();
        else
          c->del_rights(page_attribs & L4_fpage::Rights::CWSD());
      }

    return L4_fpage::Rights(0);
  }


  /// v_insert
  Obj::Insert_result v_insert(Phys_addr phys, V_pfn const &virt,
                              Page_order size, Attr page_attribs)
  FIASCO_FLATTEN
  {
    (void)size;
    assert (size == Page_order(0));

    Entry *c;

    if (Optimize_local
        && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
      {
        c = cap_virt(virt);
        if (!c)
          return Obj::Insert_err_nomem;

        Capability cap;
        if (!Mem_layout::read_special_safe((Capability*)c, cap)
            && !caps_alloc(virt))
          return Obj::Insert_err_nomem;
      }
    else
      {
        c = get_cap(virt);
        if (!c && !(c = caps_alloc(virt)))
          return Obj::Insert_err_nomem;
        Obj::set_entry(virt, c);
      }

    if (c->valid())
      {
        if (c->obj() == phys)
          {
            if (EXPECT_FALSE(c->rights() == page_attribs))
              return Obj::Insert_warn_exists;

            c->add_rights(page_attribs);
            return Obj::Insert_warn_attrib_upgrade;
          }
        else
          return Obj::Insert_err_exists;
      }

    c->set(phys, page_attribs);
    return Obj::Insert_ok;
  }


  /// lookup
  Capability lookup(Cap_index virt) FIASCO_FLATTEN
  {
    Capability *c;
    virt &= Cap_index(~(~0UL << Whole_space));

    if (SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
      c = reinterpret_cast<Capability*>(cap_virt(virt));
    else
      c = get_cap(virt);

    if (EXPECT_FALSE(!c))
      return Capability(0); // void

    return Mem_layout::read_special_safe(c);
  }

  /// lookup_local
  Kobject_iface * __attribute__((nonnull))
  lookup_local(Cap_index virt, L4_fpage::Rights *rights)
  {
    virt &= Cap_index(~(~0UL << Whole_space));
    Capability *c = reinterpret_cast<Capability*>(cap_virt(virt));
    Capability cap = Mem_layout::read_special_safe(c);
    *rights = L4_fpage::Rights(cap.rights());
    return cap.obj();
  }

  virtual V_pfn obj_map_max_address() const
  {
    Mword r;

    r = (Mem_layout::Caps_end - Mem_layout::Caps_start) / sizeof(Entry);
    if (Map_max_address < r)
      r = Map_max_address;

    return V_pfn(r);
  }

#if defined (CONFIG_JDB)
  Entry *jdb_lookup_cap(Cap_index index)
  { return get_cap(index); }
#endif // CONFIG_JDB

private:
  enum
  {
    // do not use the virtually mapped cap table in
    // v_lookup and v_insert, because the map logic needs the kernel
    // address for link pointers in the map-nodes and these addresses must
    // be valid in all address spaces.
    Optimize_local = 0,

    Whole_space = 20,
    Map_max_address = 1UL << 20, /* 20bit obj index */
  };

  static_assert(sizeof(Entry) * Map_max_address <=
                Mem_layout::Caps_end - Mem_layout::Caps_start,
                "Adapt capability mapping area");

  static Entry *cap_virt(Cap_index index)
  { return reinterpret_cast<Entry*>(Mem_layout::Caps_start) + cxx::int_value<Cap_index>(index); }

  Entry *get_cap(Cap_index index)
  {
    Mem_space *ms = SPACE::mem_space(this);

    Address phys = Address(ms->virt_to_phys((Address)cap_virt(index)));
    if (EXPECT_FALSE(phys == ~0UL))
      return 0;

    return reinterpret_cast<Entry*>(Mem_layout::phys_to_pmem(phys));
  }

  Entry *caps_alloc(Cap_index virt)
  {
    Address cv = (Address)cap_virt(virt);
    void *mem = Kmem_alloc::allocator()->q_alloc(SPACE::ram_quota(this),
                                                 Config::page_size());

    if (!mem)
      return 0;

    Obj::add_cap_page_dbg_info(mem, SPACE::get_space(this), cxx::int_value<Cap_index>(virt));

    Mem::memset_mwords(mem, 0, Config::PAGE_SIZE / sizeof(Mword));

    // Page clearing must be observable *before* the pointer to the page is
    // visible! The lookup in get_cap() happens without a lock.
    Mem::mp_wmb();

    Mem_space::Status s;
    s = SPACE::mem_space(this)->v_insert(
        Mem_space::Phys_addr(Kmem::kdir->virt_to_phys((Address)mem)),
        cxx::mask_lsb(Virt_addr(cv), Mem_space::Page_order(Config::PAGE_SHIFT)),
        Mem_space::Page_order(Config::PAGE_SHIFT),
        Mem_space::Attr(L4_fpage::Rights::RW()));
        //| Mem_space::Page_referenced | Mem_space::Page_dirty);

    switch (s)
      {
      case Mem_space::Insert_ok:
        break;
      case Mem_space::Insert_warn_exists:
      case Mem_space::Insert_warn_attrib_upgrade:
        assert (false);
        break;
      case Mem_space::Insert_err_exists:
      case Mem_space::Insert_err_nomem:
        Kmem_alloc::allocator()->q_free(SPACE::ram_quota(this),
                                        Config::page_size(), mem);
        return 0;
      };

    unsigned long cap = (unsigned long)mem | Pg::offset(cv);

    return reinterpret_cast<Entry*>(cap);
  }

protected:
  void caps_free()
  {
    Mem_space *ms = SPACE::mem_space(this);
    if (EXPECT_FALSE(!ms || !ms->dir()))
      return;

    Kmem_alloc *a = Kmem_alloc::allocator();
    for (Cap_index i = Cap_index(0); i < obj_map_max_address();
         i += Cap_diff(Obj::Caps_per_page))
      {
        Entry *c = get_cap(i);
        if (!c)
          continue;

        Obj::remove_cap_page_dbg_info(c);

        a->q_free(SPACE::ram_quota(this), Config::page_size(), c);
      }
    ms->dir()->destroy(Virt_addr(Mem_layout::Caps_start),
                       Virt_addr(Mem_layout::Caps_end-1),
                       Pdir::Super_level,
                       Pdir::Depth,
                       Kmem_alloc::q_allocator(SPACE::ram_quota(this)));
  }

};


