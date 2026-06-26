#pragma once

#include <paging-ptab-arch-bits.h>
#include <paging-attribs.h>
#include <paging-pdir.h>
#include <mem_unit.h>
#include <cxx/cxx_int>
#include <globalconfig.h>

/**
 * Mixin for PTE pointers for CPUs with virtual caches and without ASIDs.
 * (before and including ARMv5)
 */
template<typename CLASS>
struct Pte_v_cache_no_asid
{
  static bool need_cache_write_back(bool current_pt)
  { return current_pt; }

  void write_back_if(bool current_pt, Mword /*asid*/ = 0)
  {
    if (current_pt)
      Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
  }

  static void write_back(void *start, void *end)
  {
    Mem_unit::clean_dcache(start, end);
  }
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and non-coherent MMU.
 * (ARMv6 and ARMv7 without multiprocessing extension).
 */
template<typename CLASS>
struct Pte_cache_asid
{
  static bool need_cache_write_back(bool)
  { return true; }

  void write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
  {
    Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }

  static void write_back(void *start, void *end)
  {
    Mem_unit::clean_dcache(start, end);
  }
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and coherent MMU.
 * (ARMv7 with multiprocessing extension or LPAE and ARMv8).
 */
template<typename CLASS>
struct Pte_no_cache_asid
{
  static bool need_cache_write_back(bool)
  { return false; }

  static void write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
  {
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }

  static void write_back(void *, void *)
  {}
};

/**
 * Mixin for PTE pointers for 32bit page tables (short descriptors).
 */
template<typename CLASS>
class Pte_short_desc
{
public:
  enum
  {
    Max_level   = 1,
    Super_level = 0,
  };

  typedef Unsigned32 Entry;

  Unsigned32 *pte;
  unsigned char level;

  Pte_short_desc() = default;
  Pte_short_desc(void *p, unsigned char level)
  : pte((Entry *)p), level(level)
  {}

  bool is_valid() const { return access_once(pte) & 3; }
  void clear() { write_now(pte, 0); }
  bool is_leaf() const
  {
    switch (level)
      {
      case 0: return (access_once(pte) & 3) == 2;
      default: return true;
      };
  }

  Mword next_level() const
  {
    // 1 KiB second level tables
    return cxx::mask_lsb(access_once(pte), 10);
  }

  void set_next_level(Mword phys)
  {
    write_now(pte, phys | 1);
  }

  unsigned char page_order() const
  {
    if (level == 0)
      return 20; // 1 MiB
    else
      { // no tiny pages
        if ((*pte & 3) == 1)
          return 16;
        else
          return 12;
      }
  }

  Unsigned32 page_addr() const
  { return cxx::mask_lsb(*pte, page_order()); }

  Entry entry() const { return *pte; }
};

/**
 * Mixin for PTE pointers for 64bit page tables (long descriptors).
 */
template<typename CLASS>
class Pte_long_desc
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  typedef Unsigned64 Entry;

  Entry *pte;
  unsigned char level;

  Pte_long_desc() = default;
  Pte_long_desc(void *p, unsigned char level)
  : pte((Unsigned64*)p), level(level)
  {}

  bool is_valid() const { return *pte & 1; }
  void clear() { write_now(pte, 0); }
  bool is_leaf() const
  {
    if (level >= CLASS::Max_level)
      return true;
    return (*pte & 3) == 1;
  }

  Unsigned64 next_level() const
  {
    return cxx::get_lsb(cxx::mask_lsb(*pte, 12), 52);
  }

  void set_next_level(Unsigned64 phys)
  {
    // A new table was just allocated and cleared. Ensure the clearing is
    // observable to the MMU before the updated table descriptor. Otherwise the
    // next table walk might still see uninitialized PTEs.
    Mem::dmbst();
    write_now(pte, phys | 3);
  }

  Unsigned64 page_addr() const
  { return cxx::get_lsb(cxx::mask_lsb(*pte, _this()->page_order()), 52); }

  Entry entry() const { return *pte; }
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

public:
  void set_page(Entry p)
  {
    write_now(_this()->pte, p);
  }

  void set_page(Phys_mem_addr addr, Page::Attr attr)
  {
    set_page(make_page(addr, attr));
  }

  void set_attribs(Page::Attr attr)
  {
    auto p = access_once(_this()->pte);
    p = (p & _this()->_attribs_mask()) | _this()->_attribs(attr);
    write_now(_this()->pte, p);
  }

  Entry make_page(Phys_mem_addr addr, Page::Attr attr)
  {
    return _this()->_page_bits() | _this()->_attribs(attr)
           | cxx::int_value<Phys_mem_addr>(addr);
  }
};

#ifdef CONFIG_ARM_V5
template<typename M>
using Kpte_attribs_t = Pte_v5_attribs<M, Unsigned32>;

template<typename M>
using Kpte_cache_asid_t = Pte_v_cache_no_asid<M>;
#endif // CONFIG_ARM_V5

#ifdef CONFIG_ARM_V6
template<typename M>
using Kpte_cache_asid_t = Pte_cache_asid<M>;
#endif // CONFIG_ARM_V6

#ifdef CONFIG_ARM_V7
#ifdef CONFIG_MP
template<typename M>
using Kpte_cache_asid_t = Pte_no_cache_asid<M>;
#else
template<typename M>
using Kpte_cache_asid_t = Pte_cache_asid<M>;
#endif // CONFIG_MP
#endif // CONFIG_ARM_V7

#ifdef CONFIG_ARM_V8PLUS
template<typename M>
using Kpte_cache_asid_t = Pte_no_cache_asid<M>;
#endif // CONFIG_ARM_V8PLUS

#ifdef CONFIG_ARM_LPAE

template<typename M>
struct Kpte_desc_t : Pte_long_desc<M>
{
  template<typename ...T>
  Kpte_desc_t(T &&...args) : Pte_long_desc<M>(cxx::forward<T>(args)...) {}

  enum
  {
    Super_level    = K_ptab_super_level,
    Max_level      = K_ptab_max_level,
  };
};

template<typename M>
using Kpte_generic_t = Pte_generic<M, Unsigned64>;

template<typename M>
using Kpte_attribs_t = Pte_long_attribs<M, Page::Kernel_attr>;

#else // CONFIG_ARM_LPAE

template<typename M>
using Kpte_desc_t = Pte_short_desc<M>;

template<typename M>
using Kpte_generic_t = Pte_generic<M, Unsigned32>;

#ifdef CONFIG_ARM_V6PLUS

template<typename M>
using Kpte_attribs_t = Pte_v6plus_attribs<M, Page::User_attr>;

#endif // CONFIG_ARM_V6PLUS
#endif // CONFIG_ARM_LPAE

class K_pte_ptr :
  public Kpte_desc_t<K_pte_ptr>,
  public Kpte_generic_t<K_pte_ptr>,
  public Kpte_attribs_t<K_pte_ptr>,
  public Kpte_cache_asid_t<K_pte_ptr>
{
public:
  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level)
  : Kpte_desc_t<K_pte_ptr>(p, level) {}

  [[gnu::always_inline]]
  unsigned char page_order() const
  {
    return Ptab::page_order_for_level<K_ptab_traits_vpn>(level);
  }
};

class Kpdir : public Pdir_t<K_pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> {};

#ifdef CONFIG_CPU_VIRT

template<typename CLASS>
class Pte_ptr_t :
  public Pte_long_desc<CLASS>,
  public Pte_no_cache_asid<CLASS>,
  public Pte_stage2_attribs<CLASS, Page::User_attr>,
  public Pte_generic<CLASS, Unsigned64>
{
public:
  Pte_ptr_t() = default;
  Pte_ptr_t(void *p, unsigned char level) : Pte_long_desc<CLASS>(p, level) {}
};

class Pte_ptr : public Pte_ptr_t<Pte_ptr>
{
public:
  enum
  {
    Super_level = Ptab_super_level,
    Max_level   = Ptab_max_level,
  };
  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : Pte_ptr_t(p, level) {}

  unsigned char page_order() const
  { return Ptab::page_order_for_level<Ptab_traits_vpn>(level); };
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;

#else // CONFIG_CPU_VIRT

using Pte_ptr = K_pte_ptr;
using Pdir = Pdir_t<Pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn>;

#endif // CONFIG_CPU_VIRT

