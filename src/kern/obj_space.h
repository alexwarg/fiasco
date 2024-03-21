#pragma once

#include "obj_space_types.h"
#include "config.h"
#include "l4_types.h"
#include "template_math.h"
#include "assert_opt.h"
#include "mem_space.h"

#if defined (CONFIG_VIRT_OBJ_SPACE)

#include "obj_space_virt_util.h"

template<typename B>
using Obj_space_t = Obj_space_virt<B>;

#else

#include "obj_space_phys_util.h"

template<typename B>
using Obj_space_t = Obj_space_phys<B>;

#endif

class Kobject;
class Space;


template< typename SPACE >
class Generic_obj_space : Obj_space_t<Generic_obj_space<SPACE>>
{
  friend class Jdb_obj_space;
  friend class Jdb_tcb;

  using Base = Obj_space_t<Generic_obj_space<SPACE>>;


public:
  static char const * const name;

  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject *Reap_list;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;


  enum
  {
    Need_insert_tlb_flush = 0,
    Need_xcpu_tlb_flush = 0,
    Map_page_size = 1,
    Page_shift = 0,
    Map_max_address = 1UL << 20, /* 20bit obj index */
    Whole_space = 20,
    Identity_map = 0,
  };

  typedef Obj::Insert_result Status;
  static Status const Insert_ok = Obj::Insert_ok;
  static Status const Insert_warn_exists = Obj::Insert_warn_exists;
  static Status const Insert_warn_attrib_upgrade = Obj::Insert_warn_attrib_upgrade;
  static Status const Insert_err_nomem = Obj::Insert_err_nomem;
  static Status const Insert_err_exists = Obj::Insert_err_exists;

  struct Fit_size
  {
    Page_order operator () (Page_order s) const
    {
      return s >= Page_order(Obj::Caps_per_page_ld2)
             ? Page_order(Obj::Caps_per_page_ld2)
             : Page_order(0);
    }
  };

  using Base::initialize;

  static Ram_quota *ram_quota(Base const *base)
  {
    assert_opt (base);
    return static_cast<SPACE const *>(base)->ram_quota();
  }

  static Mem_space *mem_space(Base *base)
  {
    return static_cast<SPACE*>(base);
  }

  Fit_size fitting_sizes() const { return Fit_size(); }

  static Phys_addr page_address(Phys_addr o, Page_order) { return o; }
  static Phys_addr subpage_address(Phys_addr addr, V_pfc) { return addr; }
  static V_pfn page_address(V_pfn addr, Page_order) { return addr; }
  static V_pfc subpage_offset(V_pfn addr, Page_order o) { return cxx::get_lsb(addr, o); }

  static Phys_addr to_pfn(Phys_addr p) { return p; }
  static V_pfn to_pfn(V_pfn p) { return p; }
  static V_pfc to_pcnt(Page_order s) { return V_pfc(1) << s; }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }

  FIASCO_SPACE_VIRTUAL
  bool v_lookup(V_pfn const &virt, Phys_addr *phys = 0,
                Page_order *size = 0, Attr *attribs = 0) FIASCO_FLATTEN
  { return Base::v_lookup(virt, phys, size, attribs); }


  FIASCO_SPACE_VIRTUAL
  L4_fpage::Rights v_delete(V_pfn virt, Page_order size,
                            L4_fpage::Rights page_attribs)
  FIASCO_FLATTEN
  { return Base::v_delete(virt, size, page_attribs); }

  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, V_pfn const &virt, Page_order size,
                  Attr page_attribs) FIASCO_FLATTEN
  { return (Status)Base::v_insert(phys, virt, size, page_attribs); }

  bool v_fabricate(V_pfn const &address,
                   Phys_addr *phys, Page_order *size,
                   Attr* attribs = 0)
  {
    return this->v_lookup(address, phys, size, attribs);
  }

  FIASCO_SPACE_VIRTUAL
  Capability lookup(Cap_index virt) FIASCO_FLATTEN
  { return Base::lookup(virt); }

  FIASCO_SPACE_VIRTUAL
  V_pfn obj_map_max_address() const FIASCO_VIRT_OBJ_SPACE_OVERRIDE
  FIASCO_FLATTEN
  { return Base::obj_map_max_address(); }

  FIASCO_SPACE_VIRTUAL
  void caps_free() FIASCO_FLATTEN
  { Base::caps_free(); }

  Kobject_iface *lookup_local(Cap_index virt, L4_fpage::Rights *rights = 0)
  FIASCO_FLATTEN
  { return Base::lookup_local(virt, rights); }

  inline V_pfn map_max_address() const
  { return obj_map_max_address(); }

  static bool
  is_full_flush(L4_fpage::Rights rights)
  { return (bool)(rights & L4_fpage::Rights::CR()); }

  ~Generic_obj_space()
  {
    this->caps_free();
  }

  static void tlb_flush()
  {}

  static V_pfn canonize(V_pfn v)
  { return v; }


#if defined (CONFIG_JDB)
  FIASCO_SPACE_VIRTUAL Entry *jdb_lookup_cap(Cap_index index)
  { return Base::jdb_lookup_cap(index); }

  static SPACE *get_space(Base *base)
  { return static_cast<SPACE*>(base); }
#else // CONFIG_JDB

  static SPACE *get_space(Base *)
  { return 0; }

#endif // CONFIG_JDB
};

template< typename SPACE >
char const * const Generic_obj_space<SPACE>::name = "Obj_space";


