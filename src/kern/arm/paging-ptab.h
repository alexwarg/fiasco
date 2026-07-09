#pragma once

#include <paging-ptab-arch-bits.h>
#include <paging-attribs.h>
#include <paging-caching.h>
#include <paging-pdir.h>
#include <mem_unit.h>
#include <cxx/cxx_int>
#include <globalconfig.h>
#include <ptab_base-iterative.h>

#ifdef CONFIG_ARM_V5
using Kpte_cache_asid = Pte_v_cache_no_asid;
#endif // CONFIG_ARM_V5

#ifdef CONFIG_ARM_V6
using Kpte_cache_asid = Pte_cache_asid;
#endif // CONFIG_ARM_V6

#ifdef CONFIG_ARM_V7
#ifdef CONFIG_MP
using Kpte_cache_asid = Pte_no_cache_asid;
#else
using Kpte_cache_asid = Pte_cache_asid;
#endif // CONFIG_MP
#endif // CONFIG_ARM_V7

#ifdef CONFIG_ARM_V8PLUS
using Kpte_cache_asid = Pte_no_cache_asid;
#endif // CONFIG_ARM_V8PLUS

// simply always use the iterative page table
template<typename PTE_PTR, typename TRAITS, typename VA>
using Arm_pdir_t = Pdir_t<PTE_PTR, TRAITS, VA, Ptab::Iterative_base>;

template<typename ENTRY>
class Pte_page_template
{
private:
  ENTRY tmpl;

public:
  using Entry = ENTRY;
  Pte_page_template() = default;
  constexpr Pte_page_template(Entry e) : tmpl(e) {}
  constexpr Entry for_pa(Phys_mem_addr addr) const
  { return tmpl | cxx::int_value<Phys_mem_addr>(addr); }
};

template<typename ENTRY, typename CACHING,
  ENTRY PT_BITS, bool NEED_DMB,
  ENTRY VALID_MASK,
  ENTRY TYPE_MASK, ENTRY TYPE_LEAF,
  unsigned NEXT_LOW, unsigned NEXT_HIGH = sizeof(ENTRY) * 8>
struct Pte_ptr_base : CACHING
{
  using Entry = ENTRY;
  using Template = Pte_page_template<ENTRY>;

  Entry *pte;
  Ptab::Level_id level;

  Pte_ptr_base() = default;
  constexpr Pte_ptr_base(Entry *pte, Ptab::Level_id level) : pte(pte), level(level) {}

  void clear()
  {
    write_now(pte, 0);
  }

  void set(Entry e)
  {
    write_now(pte, e);
  }

  bool is_valid() const { return *pte & VALID_MASK; }
  bool is_leaf() const
  {
    if (level.get() == 0)
      return true;

    return (*pte & TYPE_MASK) == TYPE_LEAF;
  }

  Entry next_level() const
  {
    if constexpr (sizeof(ENTRY) * 8 <= NEXT_HIGH)
      return cxx::mask_lsb(*pte, NEXT_LOW);
    else
      return cxx::get_lsb(cxx::mask_lsb(*pte, NEXT_LOW), NEXT_HIGH);
  }

  void set_next_level(Entry phys)
  {
    // A new table was just allocated and cleared. Ensure the clearing is
    // observable to the MMU before the updated table descriptor. Otherwise the
    // next table walk might still see uninitialized PTEs.
    if constexpr (NEED_DMB)
      Mem::dmbst();
    write_now(pte, phys | PT_BITS);
  }

  Entry entry() const { return *pte; }

  void write_back_if(bool current_pt, Mword asid = Mem_unit::Asid_invalid)
  { CACHING::write_back_if(*this, current_pt, asid); }
};


/**
 * Mixin for PTE pointers for 32bit page tables (short descriptors).
 */
template<typename CLASS, typename CACHING>
class Pte_short_desc :
  public Pte_ptr_base<Unsigned32, CACHING, 1, false, 3, 3, 2, 10> // 1 KiB second level tables
{
public:
  using Base = Pte_ptr_base<Unsigned32, CACHING, 1, false, 3, 3, 2, 10>;
  static constexpr Ptab::Level_id Super_level{1};

  Pte_short_desc() = default;
  Pte_short_desc(void *p, Ptab::Level_id level)
  : Base{static_cast<typename Base::Entry *>(p), level}
  {}

  unsigned char page_order() const
  {
    if (this->level.get() == 1)
      return 20; // 1 MiB
    else
      { // no tiny pages
        if ((*this->pte & 3) == 1)
          return 16;
        else
          return 12;
      }
  }

  Unsigned32 page_addr() const
  { return cxx::mask_lsb(*this->pte, page_order()); }
};


/**
 * Mixin for PTE pointers for 64bit page tables (long descriptors).
 */
template<typename CLASS, typename CACHING>
class Pte_long_desc :
  public Pte_ptr_base<Unsigned64, CACHING, 3, true, 1, 3, 1, 12, 52>
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  using Base = Pte_ptr_base<Unsigned64, CACHING, 3, true, 1, 3, 1, 12, 52>;

  Pte_long_desc() = default;
  Pte_long_desc(void *p, Ptab::Level_id level)
  : Base{static_cast<Unsigned64*>(p) ,level}
  {}

  Unsigned64 page_addr() const
  { return cxx::get_lsb(cxx::mask_lsb(*this->pte, _this()->page_order()), 52); }
};


/**
 * Generic mixin for PTE pointers.
 */
template<typename CLASS, typename Entry>
class Pte_generic
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

  using Templ = Pte_page_template<Entry>;

public:
  void set_page(Phys_mem_addr addr, Page::Attr attr)
  {
    _this()->set(make_page(addr, attr));
  }

  void set_attribs(Page::Attr attr)
  {
    auto p = access_once(_this()->pte);
    p = (p & _this()->_attribs_mask()) | _this()->_attribs(attr);
    write_now(_this()->pte, p);
  }

  template<typename LEVEL_ID>
  static constexpr Templ
  make_page_tmpl(LEVEL_ID level, Page::Attr attr)
  {
    return Templ(CLASS{nullptr, level}._page_bits() | CLASS{nullptr, level}._attribs(attr));
  }

  constexpr Templ
  make_page_tmpl(Page::Attr attr) const
  {
    return Templ(_this()->_page_bits() | _this()->_attribs(attr));
  }

  Entry make_page(Phys_mem_addr addr, Page::Attr attr)
  {
    return make_page_tmpl(attr).for_pa(addr);
  }

};

#ifdef CONFIG_ARM_LPAE

template<typename M>
struct Kpte_desc_t : Pte_long_desc<M, Kpte_cache_asid>
{
  using Pte_long_desc<M, Kpte_cache_asid>::Pte_long_desc;
  static constexpr Ptab::Level_id Super_level = K_ptab_super_level;
};

template<typename M>
using Kpte_generic_t = Pte_generic<M, Unsigned64>;

template<typename M>
using Kpte_attribs_t = Pte_long_attribs<M, Page::Kernel_attr>;

#else // CONFIG_ARM_LPAE

template<typename M>
using Kpte_desc_t = Pte_short_desc<M, Kpte_cache_asid>;

template<typename M>
using Kpte_generic_t = Pte_generic<M, Unsigned32>;

#ifdef CONFIG_ARM_V5
template<typename M>
using Kpte_attribs_t = Pte_v5_attribs<M, Unsigned32>;
#endif
#ifdef CONFIG_ARM_V6PLUS
template<typename M>
using Kpte_attribs_t = Pte_v6plus_attribs<M, Page::User_attr>;
#endif // CONFIG_ARM_V6PLUS
#endif // CONFIG_ARM_LPAE

class K_pte_ptr :
  public Kpte_desc_t<K_pte_ptr>,
  public Kpte_generic_t<K_pte_ptr>,
  public Kpte_attribs_t<K_pte_ptr>
{
public:
  K_pte_ptr() = default;
  explicit K_pte_ptr(Kpte_desc_t<K_pte_ptr>::Base const &b)
  : Kpte_desc_t<K_pte_ptr>(b.pte, b.level)
  {}

  K_pte_ptr(void *p, Ptab::Level_id level)
  : Kpte_desc_t<K_pte_ptr>(p, level)
  {}

  [[gnu::always_inline]]
  unsigned char page_order() const
  {
    return Ptab::page_order_for_level<K_ptab_traits_vpn>(level);
  }
};

class Kpdir : public Arm_pdir_t<K_pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> {};

#ifdef CONFIG_CPU_VIRT

template<typename CLASS>
class Pte_ptr_t :
  public Pte_long_desc<CLASS, Pte_no_cache_asid>,
  public Pte_stage2_attribs<CLASS, Page::User_attr>,
  public Pte_generic<CLASS, Unsigned64>
{
public:
  Pte_ptr_t() = default;

  explicit Pte_ptr_t(typename Pte_long_desc<CLASS, Pte_no_cache_asid>::Base const &b)
  : Pte_long_desc<CLASS, Pte_no_cache_asid>(b.pte, b.level)
  {}

  Pte_ptr_t(void *p, Ptab::Level_id level)
  : Pte_long_desc<CLASS, Pte_no_cache_asid>(p, level)
  {}
};

class Pte_ptr : public Pte_ptr_t<Pte_ptr>
{
public:
  static constexpr Ptab::Level_id Super_level = Ptab_super_level;
  using Pte_ptr_t<Pte_ptr>::Pte_ptr_t;

  unsigned char page_order() const
  { return Ptab::page_order_for_level<Ptab_traits_vpn>(level); };
};

using Pdir = U_pdir_t<Pte_ptr>;

#else // CONFIG_CPU_VIRT

using Pte_ptr = K_pte_ptr;
using Pdir = Arm_pdir_t<Pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn>;

#endif // CONFIG_CPU_VIRT

