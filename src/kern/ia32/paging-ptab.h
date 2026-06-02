#pragma once

#include <paging-ptab-arch-bits.h>
#include <paging-pdir.h>
#include <paging-page.h>
#include <l4_fpage.h>
#include <config.h>
#include <cxx/atomic>

#include <globalconfig.h>

class Pt_entry : public Pt_entry_bits
{
public:
  static constexpr unsigned Max_level = Ptab::Level<Ptab_traits_vpn>::Max_level;
  static void have_superpages(bool yes)
  {
    _have_superpages = yes;
    _super_level = yes ? Super_level : (Super_level + 1);
  }

  static unsigned super_level()
  { return _super_level; }
#ifdef CONFIG_KERNEL_ISOLATION

  static void enable_global()
  {}

  /**
   * Global entries are entries that are not automatically flushed when the
   * page-table base register is reloaded. They are intended for kernel data
   * that is shared between all tasks.
   * @return global page-table--entry flags
   */
  static Unsigned32 global()
  { return 0; }

#else // CONFIG_KERNEL_ISOLATION

  static void enable_global()
  { _cpu_global |= Cpu_global; }

  /**
   * Global entries are entries that are not automatically flushed when the
   * page-table base register is reloaded. They are intended for kernel data
   * that is shared between all tasks.
   * @return global page-table--entry flags
   */
  static Unsigned32 global()
  { return _cpu_global; }

#endif // CONFIG_KERNEL_ISOLATION

private:
  static Unsigned32 _cpu_global;
  static unsigned _super_level;
  static bool _have_superpages;
};

class Pte_ptr : private Pt_entry
{
public:
  using Pt_entry::Super_level;
  Pte_ptr(void *pte, unsigned char level) : pte(static_cast<Mword*>(pte)), level(level) {}
  Pte_ptr() = default;

  bool is_valid() const
  { return *pte & Valid; }

  bool is_leaf() const
  { return level == Max_level || (*pte & Pse_bit); }

  /**
   * \pre is_leaf() == false
   */
  Mword next_level() const
  { return cxx::mask_lsb(*pte, (unsigned)Config::PAGE_SHIFT); }

  /**
   * \pre cxx::get_lsb(phys_addr, Config::PAGE_SHIFT) == 0
   */
  void set_next_level(Mword phys_addr)
  { *pte = phys_addr | Valid | User | Writable; }

  void set_page(Mword phys, Mword attr)
  {
    Mword v = phys | Valid | attr;
    if (level < Max_level)
      v |= Pse_bit;
    *pte = v;
  }

  Pte_ptr const &operator ++ ()
  {
    ++pte;
    return *this;
  }

  unsigned char page_order() const
  { return Ptab::page_order_for_level<Ptab_traits_vpn>(level); }

  Mword page_addr() const
  { return cxx::mask_lsb(*pte, page_order()) & ~Mword(XD); }

  void set_attribs(Page::Attr attr)
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;
    Mword r = 0;
    if (attr.rights & R::W()) r |= Writable;
    if (attr.rights & R::U()) r |= User;
    if (!(attr.rights & R::X())) r |= XD;
    if (attr.type == T::Normal()) r |= Page::CACHEABLE;
    if (attr.type == T::Buffered()) r |= Page::BUFFERED;
    if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
    if (attr.kern & K::Global()) r |= global();
    *pte = (*pte & ~(ATTRIBS_MASK | Page::Cache_mask)) | r;
  }

  Mword make_page(Phys_mem_addr addr, Page::Attr attr)
  {
    Mword r = (level < Max_level) ? (Mword)Pse_bit : 0;
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;
    if (attr.rights & R::W()) r |= Writable;
    if (attr.rights & R::U()) r |= User;
    if (!(attr.rights & R::X())) r |= XD;
    if (attr.type == T::Normal()) r |= Page::CACHEABLE;
    if (attr.type == T::Buffered()) r |= Page::BUFFERED;
    if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
    if (attr.kern & K::Global()) r |= global();
    return cxx::int_value<Phys_mem_addr>(addr) | r | Valid;
  }

  void set_page(Mword p)
  {
    write_now(pte, p);
  }

  void set_page(Phys_mem_addr addr, Page::Attr attr)
  {
    set_page(make_page(addr, attr));
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;

    Mword _raw = *pte;
    R r = R::R();
    if (_raw & Writable) r |= R::W();
    if (_raw & User) r |= R::U();
    if (!(_raw & XD)) r |= R::X();

    T t;
    switch (_raw & Page::Cache_mask)
      {
      default:
      case Page::CACHEABLE:    t = T::Normal(); break;
      case Page::BUFFERED:     t = T::Buffered(); break;
      case Page::NONCACHEABLE: t = T::Uncached(); break;
      }
    // do not care for kernel special flags, as this is used for user
    // level mappings
    return Page::Attr(r, t);
  }

  void add_attribs(Mword attr)
  { *pte |= attr; }

  L4_fpage::Rights access_flags() const
  {
    if (!is_valid())
      return L4_fpage::Rights(0);

    L4_fpage::Rights r;
    for (;;)
      {
        auto raw = *pte;

        if (raw & Dirty)
          r = L4_fpage::Rights::RW();
        else if (raw & Referenced)
          r = L4_fpage::Rights::R();
        else
          return L4_fpage::Rights(0);

        if (cxx::atomic_compare_exchange_strong(pte, raw, raw & ~(Dirty | Referenced)))
          return r;
      }
  }

  void clear()
  { *pte = 0; }

  static void write_back(void *, void *)
  {}

  static void write_back_if(bool)
  {}

  void del_attribs(Mword attr)
  { *pte &= ~attr; }

  void del_rights(L4_fpage::Rights r)
  {
    if (r & L4_fpage::Rights::W())
      *pte &= ~Writable;

    if (r & L4_fpage::Rights::X())
      *pte |= XD;
  }


  typedef Mword Entry;
  Entry *pte;
  Entry entry() const { return *pte; }
  unsigned char level;
};

using Pdir = Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn>;
class Kpdir : public Pdir {};

