#pragma once

#include "assert_opt.h"
#include "obj_space_types.h"
#include "ram_quota.h"
#include "cxx/type_traits"

#include <cstring>
#include <cassert>

#include "config.h"
#include "cpu.h"
#include "kmem_alloc.h"
#include "mem.h"
#include "mem_layout.h"
#include "ram_quota.h"

#include "globalconfig.h"

template< typename SPACE >
class Obj_space_phys
{
public:
  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;

  Obj_space_phys() : _dir(nullptr)
  {}

  bool initialize()
  { return alloc_dir(); }

  bool v_lookup(V_pfn const &virt, Phys_addr *phys,
                Page_order *size, Attr *attribs) FIASCO_FLATTEN
  {
    if (size) *size = Page_order(0);
    Entry *cap = get_cap(virt);

    if (EXPECT_FALSE(!cap))
      {
        if (size) *size = Page_order(Obj::Caps_per_page_ld2);
        return false;
      }

    Capability c = cap->capability();

    Obj::set_entry(virt, cap);
    if (phys) *phys = c.obj();
    if (c.valid() && attribs) *attribs = cap->rights();
    return c.valid();
  }

  L4_fpage::Rights v_delete(V_pfn virt, Page_order size,
                            L4_fpage::Rights page_attribs)
  FIASCO_FLATTEN
  {
    (void)size;
    assert (size == Page_order(0));
    Entry *c = get_cap(virt);

    if (c && c->valid())
      {
        if (page_attribs & L4_fpage::Rights::CR())
          c->invalidate();
        else
          c->del_rights(page_attribs);
      }

    return L4_fpage::Rights(0);
  }

  Obj::Insert_result v_insert(Phys_addr phys, V_pfn const &virt,
                              Page_order size, Attr page_attribs)
  FIASCO_FLATTEN
  {
    (void)size;
    assert (size == Page_order(0));

    Entry *c = get_cap(virt);

    if (!c && !(c = caps_alloc(virt)))
      return Obj::Insert_err_nomem;

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

    Obj::set_entry(virt, c);
    c->set(phys, page_attribs);
    return Obj::Insert_ok;
  }

  Capability lookup(Cap_index virt) FIASCO_FLATTEN
  {
    Entry *c = get_cap(virt);

    if (EXPECT_FALSE(!c))
      return Capability(0); // void

    return c->capability();
  }

  class Cap_ref : public Obj::Cap_reference<Cap_ref>
  {
  public:
    using Obj::Cap_reference<Cap_ref>::Cap_reference;
    static Capability read_cap_safely(Capability const *c)
    { return access_once(c); }
  };


  Cap_ref
  lookup_local(Cap_index virt, L4_fpage::Rights expected)
  {
    Entry *c = get_cap(virt);

    if (EXPECT_FALSE(!c))
      return nullptr;

    return Cap_ref(&c->capability(), expected);
  }

  void caps_free()
  {
    if (!_dir)
      return;

    Cap_dir *d = _dir;
    _dir = 0;

    Kmem_alloc *a = Kmem_alloc::allocator();
    for (unsigned i = 0; i < Slots_per_dir; ++i)
      {
        if (!d->d[i])
          continue;

        Obj::remove_cap_page_dbg_info(d->d[i]);
        a->q_free(ram_quota(), Config::page_size(), d->d[i]);
      }

    a->q_free(ram_quota(), Config::page_size(), d);
  }

  V_pfn obj_map_max_address() const
  {
    return V_pfn(Slots_per_dir * Obj::Caps_per_page);
  }

#if defined (CONFIG_JDB)
  Entry *jdb_lookup_cap(Cap_index index)
  { return get_cap(index); }
#endif // CONFIG_JDB


private:
  enum
  {
    Slots_per_dir = Config::PAGE_SIZE / sizeof(void*)
  };

  struct Cap_table { Entry e[Obj::Caps_per_page]; };
  struct Cap_dir   { Cap_table *d[Slots_per_dir]; };
  Cap_dir *_dir;

  Ram_quota *ram_quota() const
  {
    assert_opt (this);
    return SPACE::ram_quota(this);
  }

  bool alloc_dir()
  {
    static_assert(sizeof(Cap_dir) == Config::PAGE_SIZE, "cap_dir size mismatch");
    auto *dir = Kmem_alloc::allocator()->q_alloc<Cap_dir>(ram_quota(),
                                                          Config::page_size());
    if (dir)
      Mem::memset_mwords(dir, 0, Config::PAGE_SIZE / sizeof(Mword));

    // Page clearing must be observable *before* the pointer to the table is
    // visible! The lookup in get_cap() happens without a lock.
    Mem::mp_wmb();

    _dir = dir;
    return _dir;
  }

  Entry *get_cap(Cap_index index)
  {
    if (EXPECT_FALSE(!_dir))
      return 0;

    unsigned d_idx = cxx::int_value<Cap_index>(index) >> Obj::Caps_per_page_ld2;
    if (EXPECT_FALSE(d_idx >= Slots_per_dir))
      return 0;

    Cap_table *tab = _dir->d[d_idx];

    if (EXPECT_FALSE(!tab))
      return 0;

    unsigned offs  = cxx::get_lsb(cxx::int_value<Cap_index>(index), Obj::Caps_per_page_ld2);
    return &tab->e[offs];
  }

  Entry *caps_alloc(Cap_index virt)
  {
    if (EXPECT_FALSE(!_dir && !alloc_dir()))
      return 0;

    static_assert(sizeof(Cap_table) == Config::PAGE_SIZE, "cap table size mismatch");
    unsigned d_idx = cxx::int_value<Cap_index>(virt) >> Obj::Caps_per_page_ld2;
    if (EXPECT_FALSE(d_idx >= Slots_per_dir))
      return 0;

    void *mem = Kmem_alloc::allocator()->q_alloc(ram_quota(), Config::page_size());

    if (!mem)
      return 0;

    Obj::add_cap_page_dbg_info(mem, SPACE::get_space(this),  cxx::int_value<Cap_index>(virt));

    Mem::memset_mwords(mem, 0, Config::PAGE_SIZE / sizeof(Mword));

    // Page clearing must be observable *before* the pointer to the table is
    // visible! The lookup in get_cap() happens without a lock.
    Mem::mp_wmb();

    Cap_table *tab = _dir->d[d_idx] = reinterpret_cast<Cap_table*>(mem);
    return &tab->e[ cxx::get_lsb(cxx::int_value<Cap_index>(virt), Obj::Caps_per_page_ld2)];
  }
};

#if defined (CONFIG_VIRT_OBJ_SPACE)
// ------------------------------------------------------------------------

/**
 * Allows to override the virtually mapped object space Space
 * by the multi-level table based structure.
 *
 * This is useful for Vm or Io spaces that never run threads, and
 * saves the overhead of software page-table walks and phys-to-virt
 * translations for capability lookup.
 */
template<typename BASE>
class Obj_space_phys_override :
  public BASE,
  Obj_space_phys< Obj_space_phys_override<BASE> >
{
  typedef Obj_space_phys< Obj_space_phys_override<BASE> > Obj_space;

public:
  bool initialize()
  {
    return BASE::initialize() && Obj_space::initialize();
  }

  using BASE::ram_quota;
  static Ram_quota *ram_quota(Obj_space const *obj_sp)
  { return static_cast<Obj_space_phys_override<BASE> const *>(obj_sp)->ram_quota(); }

  bool FIASCO_FLATTEN
  v_lookup(typename Obj_space::V_pfn const &virt,
           typename Obj_space::Phys_addr *phys,
           typename Obj_space::Page_order *size,
           typename Obj_space::Attr *attribs) override
  { return Obj_space::v_lookup(virt, phys, size, attribs); }

  L4_fpage::Rights FIASCO_FLATTEN
  v_delete(typename Obj_space::V_pfn virt,
           typename Obj_space::Page_order size,
           L4_fpage::Rights page_attribs) override
  { return Obj_space::v_delete(virt, size, page_attribs); }

  Obj::Insert_result FIASCO_FLATTEN
  v_insert(typename Obj_space::Phys_addr phys,
           typename Obj_space::V_pfn const &virt,
           typename Obj_space::Page_order size,
           typename Obj_space::Attr page_attribs) override
  { return Obj_space::v_insert(phys, virt, size, page_attribs); }

  typename Obj_space::Capability FIASCO_FLATTEN
  lookup(Cap_index virt) override
  { return Obj_space::lookup(virt); }

  typename Obj_space::V_pfn FIASCO_FLATTEN
  obj_map_max_address() const override
  { return Obj_space::obj_map_max_address(); }

  void FIASCO_FLATTEN caps_free() override
  { Obj_space::caps_free(); }

  template<typename ...ARGS>
  Obj_space_phys_override(ARGS &&...args) : BASE(cxx::forward<ARGS>(args)...) {}

  ~Obj_space_phys_override() { caps_free(); }

#if defined (CONFIG_JDB)
  static inline Obj_space_phys_override *
  get_space(Obj_space *base)
  { return static_cast<Obj_space_phys_override *>(base); }

  Obj::Entry *jdb_lookup_cap(Cap_index index) override
  { return Obj_space::jdb_lookup_cap(index); }

#else // CONFIG_JDB
  static Obj_space_phys_override *
  get_space(Obj_space *)
  { return 0; }
#endif // CONFIG_JDB
};

#else // CONFIG_VIRT_OBJ_SPACE
// ------------------------------------------------------------------------


/**
 * The noop version when Space already uses a multi-level array for
 * the object space.
 */
template<typename BASE>
class Obj_space_phys_override : public BASE
{
public:
  template<typename ...ARGS>
  Obj_space_phys_override(ARGS &&...args) : BASE(cxx::forward<ARGS>(args)...) {}
};

#endif // CONFIG_VIRT_OBJ_SPACE

