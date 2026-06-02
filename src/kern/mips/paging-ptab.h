#pragma once

#include <paging-tlb-entry.h>
#include <paging-page.h>
#include <ptab_base.h>

#include <cxx/cxx_int>
#include <config.h>
#include <globalconfig.h>

class Pdir_defs_32
{
public:
  enum : Mword
  {
    Used_levels = 2, // must be consistent with the used slots in PWSize
    Depth = 4,       // for JDB, it skips zero bit levels itself
    PT_level = 3,    // Level index for the final page table
    PWField_ptei = 6,
    PWField = (21 << 24) | (0 << 18) | (0 << 12) | (Config::PAGE_SHIFT        << 6) | PWField_ptei,
    PWSize  = (11 << 24) | (0 << 18) | (0 << 12) | ((21 - Config::PAGE_SHIFT) << 6) | 0,

    PWCtl_psn = 0
  };
};

class Pdir_defs_64
{
public:
  enum : Mword
  {
    Used_levels = 4, // must be consistent with the used slots in PWSize
    Depth = 4,       // for JDB, it skips zero bit levels itself
    PT_level = 3,    // Level index for the final page table
    PWField_ptei = 6,
    PWField = (39 << 24) | (30 << 18) | (21 << 12) | (Config::PAGE_SHIFT        << 6) | PWField_ptei,
    PWSize  = ( 9 << 24) | ( 9 << 18) | ( 9 << 12) | ((21 - Config::PAGE_SHIFT) << 6) | 0,

    PWCtl_psn = 0
  };
};

#ifdef CONFIG_BIT32
using Pdir_defs = Pdir_defs_32;
#endif
#ifdef CONFIG_BIT64
using Pdir_defs = Pdir_defs_64;
#endif

class Pdir : public Pdir_defs
{
public:
  typedef Page_number Va;
  typedef Page_count  Vs;
  typedef Addr::Addr<Config::PAGE_SHIFT> Phys_addr;

  void clear()
  {
    for (auto &e: _entries)
      e = Leaf;
  }

  Address virt_to_phys(Address virt) const;


  struct Walk_res
  {
    Mword *e;
    unsigned size;

  private:
    static Mword _rights(L4_fpage::Rights r)
    {
      typedef L4_fpage::Rights R;
      Mword v = 0;
      if (r & R::W())    v |= Write;
      if (!(r & R::X())) v |= XI;
      if (!(r & R::R())) v |= RI;
      return v;
    }

    static Mword _attr(Page::Attr attr)
    {
      typedef Page::Type T;
      typedef Page::Kern K;
      Mword v = 0;
      if (attr.type == T::Normal())   v = Tlb_entry::cached   << PWField_ptei;
      if (attr.type == T::Buffered()) v = Tlb_entry::C_UCA    << PWField_ptei;
      if (attr.type == T::Uncached()) v = Tlb_entry::Uncached << PWField_ptei;
      if (attr.kern & K::Global())    v |= Tlb_entry::Global  << PWField_ptei;
      v |= _rights(attr.rights);
      return v;
    }

  public:
    bool is_pte() const { return *e & Valid; }
    bool is_writable() const { return *e & Write; }
    Phys_addr page_addr() const
    {
      return Phys_addr((*e << (6 - PWField_ptei)) & (~0UL << 12));
    }

    L4_fpage::Rights rights() const
    {
      typedef L4_fpage::Rights R;
      R r = R::U();
      if (!(*e & RI)) r |= R::R();
      if (*e & Write) r |= R::W();
      if (!(*e & XI)) r |= R::X();
      return r;
    }

    Page::Attr attribs() const
    {
      typedef Page::Type T;
      typedef Page::Kern K;

      Page::Attr a;
      a.type = T::Normal();
      a.rights = rights();
      a.kern = K();
      Mword v = *e;
      Mword ct = (v >> PWField_ptei) & Tlb_entry::Cache_mask;
      if (ct == Tlb_entry::cached)
        a.type = T::Normal();
      else if (ct == Tlb_entry::Uncached)
        a.type = T::Uncached();
      else if (ct == Tlb_entry::C_UCA)
        a.type = T::Buffered();

      return a;
    }

    static Mword make_page(Phys_addr pa, Page::Attr attr)
    {
      Mword v = cxx::int_value<Phys_addr>(pa);
      v >>= 6 - PWField_ptei;
      v |= _attr(attr) | Valid | Leaf;
      return v;
    }

    static Mword del_rights(Mword orig, L4_fpage::Rights del)
    {
      Mword r = _rights(del) ^ Rights_neg_mask;
      orig ^= Rights_neg_mask;
      orig &= ~r;
      orig ^= Rights_neg_mask;
      return orig;
    }

    void clear()
    {
      *e = Leaf;
    }
  };

  enum : Mword
  {
    Valid = Tlb_entry::Valid << PWField_ptei,
    XI    = 1UL << (PWField_ptei - 2),
    RI    = 2UL << (PWField_ptei - 2),
    Write = Tlb_entry::Write << PWField_ptei,
    Leaf  = 1UL << PWCtl_psn,

    Rights_mask     = Write | XI | RI,
    Rights_neg_mask = XI | RI,

    Entries = 1UL << ((PWSize >> 24) & 0x3f),
  };

  static constexpr unsigned l_size(unsigned lvl)
  { return (PWSize >> ((4 - lvl) * 6)) & 0x3f; }

  static constexpr unsigned l_field(unsigned lvl)
  { return (PWField >> ((4 - lvl) * 6)) & 0x3f; }

  template<typename ALLOC = Ptab::Null_alloc>
  Walk_res walk(Va _virt, unsigned isize = 0,
                ALLOC const &alloc = ALLOC())
  {
    Mword virt = cxx::int_value<Virt_addr>(_virt);
    Mword *p = _entries;
    Walk_res r;
    r.size = 32; // full space at base ptr level

    bool do_alloc = false;

    for (unsigned lvl = 0; lvl < 4; ++lvl)
      {
        auto const size = l_size(lvl);
        if (!size)
          continue;

        if (do_alloc)
          {
            p = static_cast<Mword*>(alloc.alloc(Bytes(sizeof(Mword) << size)));
            if (!p)
              return r; // OOM

            for (unsigned i = 0; i < (1UL << size); ++i)
              p[i] = Leaf;

            *r.e = reinterpret_cast<Mword>(p);
          }

        r.size = l_field(lvl);
        Mword const mask = (1UL << size) - 1;
        auto idx = (virt >> r.size) & mask;
        r.e = &p[idx];
        if (*r.e & Leaf)
          {
            if (r.size == isize || r.is_pte())
              return r;

            if (!alloc.valid())
              return r;

            do_alloc = true;
            continue;
          }
        p = reinterpret_cast<Mword*>(*r.e);
      }

    return r;
  }

  template<typename ALLOC>
  void destroy(Va _start, Va _end, ALLOC const &alloc);

  Mword _entries[Entries];

  // for JDB
  struct Pte_ptr
  {
    typedef Mword Entry;
    Mword *e;
    unsigned char level;
    Pte_ptr() = default;
    Pte_ptr(void *p, unsigned char lvl) : e(static_cast<Mword *>(p)), level(lvl) {}
    bool is_leaf() const  { return *e & Leaf; }
    bool is_valid() const { return !(*e & Leaf) || (*e & Valid); }
    Address next_level() const { return *e; }
    Address page_addr() const
    { return (*e << (6 - PWField_ptei)) & (~0UL << 12); }
  };

  struct Levels
  {
    static constexpr unsigned entry_size(unsigned)
    { return sizeof(Mword); }

    static constexpr unsigned long length(unsigned l)
    { return 1UL << l_size(l); }
  };

  static constexpr unsigned lsb_for_level(unsigned l)
  { /* Va relative */ return l_field(l) - l_field(PT_level); }
  // end JDB
};


template<typename ALLOC>
void
Pdir::destroy(Va _start, Va _end, ALLOC const &alloc)
{
  // nothing to free for a single level array
  if (Used_levels < 2)
    return;

  // 1. this code supports level 0 aligned granularity only
  // 2. Assume that the first entry in PWSize is valid not 0

  Mword start = cxx::int_value<Virt_addr>(_start);
  Mword end   = cxx::int_value<Virt_addr>(_end);

  Mword l_mask[Used_levels];
  Mword l_shift[Used_levels];
  Mword l_bits[Used_levels];
  Mword *p[Used_levels - 1];

  unsigned n = 0;
  for (unsigned l = 0; l < 4; ++l)
    {
      auto const s = l_size(l);
      if (!s)
        continue;

      l_bits[n] = s;
      l_mask[n] = (1UL << s) - 1;
      l_shift[n] = l_field(l);
      ++n;
    }

  n = 0;
  p[0] = _entries;
  while (start <= end)
    {
      auto idx = (start >> l_shift[n]) & l_mask[n];
      Mword v = p[n][idx];
      if (!(v & Leaf))
        {
          if (n < Used_levels - 2)
            {
              ++n;
              p[n] = reinterpret_cast<Mword *>(v);
              continue;
            }

          alloc.free((void *)v, Bytes(sizeof(Mword) << l_bits[n + 1]));
        }

      if (start >= end)
        break;

      start += (1UL << l_shift[n]);
      while (n && ((start >> l_shift[n]) & l_mask[n]) == 0)
        {
          alloc.free((void *)(p[n]), Bytes(sizeof(Mword) << l_bits[n]));
          --n;
        }
    }

  while (n)
    {
      alloc.free((void *)(p[n]), Bytes(sizeof(Mword) << l_bits[n]));
      --n;
    }
}

