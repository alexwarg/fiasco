
#pragma once

#include <ptab_base.h>

namespace Ptab {
namespace Recursive_bits {

template< typename TRAITS >
struct Entry_vec
{
  typedef typename TRAITS::Entry Entry;
  enum
  {
    Length = 1UL << TRAITS::Size,
    Size   = TRAITS::Size,
    Mask   = TRAITS::Mask,
    Shift  = TRAITS::Shift,
  };

  Entry _e[Length];

  static unsigned idx(Address virt)
  {
    if (Mask)
      return cxx::get_lsb(virt >> Shift, Address{Size});
    else
      return (virt >> Shift);
  }

  Entry &operator [] (unsigned idx) { return _e[idx]; }
  Entry const &operator [] (unsigned idx) const { return _e[idx]; }

  template<typename PTE_PTR>
  void clear(Level_id level, bool force_write_back)
  {
    for (unsigned i=0; i < Length; ++i)
      PTE_PTR(&_e[i], level).clear();

    if (force_write_back)
      PTE_PTR::write_back(&_e[0], &_e[Length]);
  }
};

template<typename TRAITS, typename PTE_PTR, unsigned LEVEL>
class Pt_level_impl
{
private:
  using Ld = Ldesc<TRAITS::Base_shift, sizeof(typename TRAITS::Entry)>;
  using Li = Pt_gen_level_impl<PTE_PTR, typename TRAITS::Entry, Ld>;

public:
  using Self = Pt_level_impl<TRAITS, PTE_PTR, LEVEL>;
  using Entry = typename TRAITS::Entry;
  using Vec = Entry_vec<TRAITS>;
  using Template = typename PTE_PTR::Template;


  static constexpr Ld ld() { return Ld{TRAITS::Shift, TRAITS::Size, false, true}; }
  static constexpr Level_id level_id{LEVEL};
  Vec _e;

  void clear(bool force_write_back)
  { Li(_e._e).clear(level_id, ld(), force_write_back); }

  PTE_PTR get_entry(Address virt)
  { return Li(_e._e).get_entry(level_id, ld(), virt); }

  template< typename MEM >
  void unmap(Address start, Address end, unsigned, bool force_write_back, MEM &&)
  { Li(_e._e).unmap(level_id, ld(), start, end, force_write_back); }

  template< typename Phys_addr, typename ALLOC, typename MEM >
  auto map(Phys_addr phys, Address virt, Address virt_end,
           Template tmpl, unsigned, bool force_write_back,
           ALLOC &&, MEM &&) -> decltype(phys - phys)
  {
    return Li(_e._e).map(level_id, ld(), phys, virt, virt_end, tmpl,
                         force_write_back);
  }

  static void skip(Address &virt, unsigned long &size)
  { Li::skip(level_id, ld(), virt, size); }

  template< typename ALLOC, typename MEM >
  void destroy(Address, Address, unsigned, unsigned, ALLOC &&, MEM &&)
  {}

  template< typename ALLOC, typename MEM >
  int sync(Address &l_addr, Self const &_r, Address &r_addr,
           Address &size, unsigned, bool force_write_back, ALLOC &&, MEM &&)
  { return Li(_e._e).sync(level_id, ld(), l_addr, _r._e._e, r_addr, size, force_write_back); }
};

template< typename LAST, typename PTE_PTR>
class Walk;

template< typename LAST, typename PTE_PTR>
class Walk<List<LAST>, PTE_PTR> : public Pt_level_impl<LAST, PTE_PTR, 0>
{
public:
  enum { Level = 0 };
  using Impl = Pt_level_impl<LAST, PTE_PTR, 0>;
  using Entry = typename LAST::Entry;
  using Traits = LAST;

  template< typename ALLOC, typename MEM >
  PTE_PTR walk(Address virt, unsigned, bool, ALLOC &&, MEM &&)
  { return this->get_entry(virt); }
};

template< typename HEAD, typename ...TAIL, typename PTE_PTR>
class Walk<List<HEAD, TAIL...>, PTE_PTR>
{
public:
  using Next = Walk<List<TAIL...>, PTE_PTR>;
  using Entry = typename HEAD::Entry;
  using Traits = HEAD;
  using Template = typename PTE_PTR::Template;

  enum { Level = Next::Level + 1 };
  using Impl = Pt_level_impl<HEAD, PTE_PTR, Level>;

private:
  Impl _impl;
  using Self = Walk<List<HEAD, TAIL...>, PTE_PTR>;

  template< typename ALLOC >
  Next *alloc_next(PTE_PTR e, ALLOC &&a, bool force_write_back)
  {
    Next *n = static_cast<Next*>(a.alloc(Bytes(sizeof(Next))));
    if (EXPECT_FALSE(!n))
      return 0;

    n->clear(force_write_back);
    e.set_next_level(a.to_phys(n));
    e.write_back_if(force_write_back);

    return n;
  }

public:
  template< typename MEM >
  void unmap(Address start, Address end, unsigned level,
             bool force_write_back, MEM &&mem)
  {
    if (level == Level)
      {
        _impl.unmap(start, end, level, force_write_back, cxx::forward<MEM>(mem));
        return;
      }

    auto desc = Impl::ld(); //d[cur_level.get()];
    auto index_end = desc.index(end);
    for (auto index = desc.index(start);; ++index, start = index << desc.shift())
      {
        PTE_PTR e = _impl.get_entry(start);
        if (!e.is_valid() || e.is_leaf())
          continue;

        Address ve;
        if (index < index_end)
          ve = ((index + 1) << desc.shift()) - 1;
        else
          ve = end;

        Next *n = reinterpret_cast<Next *>(mem.phys_to_pmem(e.next_level()));
        n->unmap(start, ve, level, force_write_back, mem);

        if (index >= index_end)
          return;
      }
  }

  template< typename Phys_addr, typename ALLOC, typename MEM >
  [[nodiscard]]
  auto map(Phys_addr phys, Address virt, Address virt_end,
           Template tmpl, unsigned level, bool force_write_back,
           ALLOC &&alloc, MEM &&mem) -> decltype(phys - phys)
  {
    using Mapped_size = decltype(phys - phys);
    if (level == Level)
      return _impl.map(phys, virt, virt_end, tmpl, level,
                       force_write_back, cxx::forward<ALLOC>(alloc),
                       cxx::forward<MEM>(mem));

    auto desc = Impl::ld(); //d[cur_level.get()];
    auto virt_index_end = desc.index(virt_end);
    for (auto virt_index = desc.index(virt);;)
      {
        PTE_PTR e = _impl.get_entry(virt);
        Next *n;
        if (!e.is_valid())
          {
            if (!alloc.valid() || !(n = alloc_next(e, alloc, force_write_back)))
              return Mapped_size{0};
          }
        else if (desc.may_be_leaf() && e.is_leaf())
          return Mapped_size{0};
        else
          n = reinterpret_cast<Next *>(mem.phys_to_pmem(e.next_level()));

        Address ve;
        if (virt_index < virt_index_end)
          ve = ((virt_index + 1) << desc.shift()) - 1;
        else
          ve = virt_end;

        Mapped_size mapped_size =
          n->map(phys, virt, ve, tmpl, level, force_write_back,
                 alloc, mem);

        if (!mapped_size)
          return mapped_size;

        if (virt_index >= virt_index_end)
          return mapped_size;

        ++virt_index;
        virt = Address{virt_index} << desc.shift();
        phys += mapped_size;
      }
  }

  template< typename ALLOC, typename MEM >
  void destroy(Address start, Address end,
               unsigned start_level, unsigned end_level,
               ALLOC &&alloc, MEM &&mem)
  {
    //printf("destroy: %*.s%lx-%lx lvl=%d:%d depth=%d\n", Depth*2, "            ", start, end, start_level, end_level, Depth);
    if (!alloc.valid() || Level <= end_level)
      return;

    unsigned idx_start = Impl::Vec::idx(start);
    unsigned idx_end = Impl::Vec::idx(end) + 1;
    //printf("destroy: %*.sidx: %d:%d\n", Depth*2, "            ", idx_start, idx_end);

    for (unsigned idx = idx_start; idx < idx_end; ++idx)
      {
        PTE_PTR e(&_impl._e[idx], Level_id(Level));
        if (!e.is_valid() || (HEAD::May_be_leaf && e.is_leaf()))
          continue;

        Next *n = reinterpret_cast<Next*>(mem.phys_to_pmem(e.next_level()));
        n->destroy(idx > idx_start ? 0 : start,
                   idx + 1 < idx_end ? (1UL << Traits::Shift)-1 : end,
                   start_level, end_level, alloc, mem);
        if (Level <= start_level)
          {
            //printf("destroy: %*.sfree: %p: %p(%zd)\n", Depth*2, "            ", this, n, sizeof(Next));
            alloc.free(n, Bytes(sizeof(Next)));
          }
      }
  }

  template< typename ALLOC, typename MEM >
  int sync(Address &l_a, Self const &_r, Address &r_a,
           Address &size, unsigned level, bool force_write_back,
           ALLOC &&alloc, MEM &&mem)
  {
    if (level == Level)
      return _impl.sync(l_a, _r._impl, r_a, size, Level,
               force_write_back, cxx::forward<ALLOC>(alloc),
               cxx::forward<MEM>(mem));

    unsigned count = size >> Traits::Shift;
      {
        unsigned const lx = Impl::Vec::idx(l_a);
        unsigned const rx = Impl::Vec::idx(r_a);
        unsigned const mx = lx > rx ? lx : rx;
        if (mx + count >= Impl::Vec::Length)
          count = Impl::Vec::Length - mx;
      }

    bool need_flush = false;

    for (unsigned i = count; size && i > 0; --i) //while (size)
      {
        PTE_PTR l = _impl.get_entry(l_a);
        PTE_PTR r = const_cast<Self &>(_r)._impl.get_entry(r_a);
        if (!r.is_valid())
          {
            l_a += 1UL << Traits::Shift;
            r_a += 1UL << Traits::Shift;
            if (size > 1UL << Traits::Shift)
              {
                size -= 1UL << Traits::Shift;
                continue;
              }
            break;
          }

        Next *n = nullptr;
        if (!l.is_valid())
          {
            if (!alloc.valid() || !(n = alloc_next(l, alloc, force_write_back)))
              return -1;
          }
        else
          n = reinterpret_cast<Next*>(mem.phys_to_pmem(l.next_level()));

        Next *rn = reinterpret_cast<Next*>(mem.phys_to_pmem(r.next_level()));

        int err = n->sync(l_a, *rn, r_a, size, level, force_write_back, alloc, mem);
        if (err > 0)
          need_flush = true;

        if (err < 0)
          return err;
      }

    return need_flush;
  }

  void clear(bool force_write_back)
  { _impl.clear(force_write_back); }

  template< typename ALLOC, typename MEM >
  PTE_PTR walk(Address virt, unsigned level, bool force_write_back, ALLOC &&alloc, MEM &&mem)
  {
    PTE_PTR e = _impl.get_entry(virt);

    if (level == Level)
      return e;
    else if (!e.is_valid())
      {
        Next *n;
        if (alloc.valid() && (n = alloc_next(e, alloc, force_write_back)))
          return n->walk(virt, level, force_write_back,
                         cxx::forward<ALLOC>(alloc),
                         cxx::forward<MEM>(mem));
        else
          return e;
      }
    else if (e.is_leaf())
      return e;
    else
      {
        Next *n = reinterpret_cast<Next*>(mem.phys_to_pmem(e.next_level()));
        return n->walk(virt, level, force_write_back,
                       cxx::forward<ALLOC>(alloc),
                       cxx::forward<MEM>(mem));
      }
  }
};

} // namespace Recursive_bits

template<typename PTE_PTR, typename ADDR, typename MEM_DFLT, typename TRAITS>
class Base
{
public:
  using Va = typename ADDR::Value_type;
  using Vs = typename ADDR::Value_type::Diff_type;
  using Traits = TRAITS;
  using Pte_ptr = PTE_PTR;
  using Addr = ADDR;
  using Mem_default = MEM_DFLT;
  using L0 = typename Level<TRAITS>::Traits;
  using Level_id = Ptab::Level_id;

  enum
  {
    Base_shift = L0::Base_shift,
  };

  static constexpr Address max_addr()
  {
    // Attention: Must use 64 bit arithmetic because some page tables (namely
    // ia32 EPT) have more virtual address bits than what fits into the
    // Address type.
    return static_cast<Address>(~0ULL >> (sizeof(unsigned long long) * 8
                                         - L0::Base_shift
                                         - L0::Shift
                                         - L0::Size));
  }

private:
  using Walk = Ptab::Recursive_bits::Walk<TRAITS, PTE_PTR>;
  using Levels = Level<Traits>;

public:
  static constexpr unsigned size()
  { return sizeof(Walk); }

  static constexpr unsigned depth()
  { return static_cast<unsigned>(Walk::Level); }

  static constexpr Level_id root_level()
  { return Level_id(static_cast<unsigned>(Walk::Level)); }

  static constexpr Level_id from_root_level(unsigned l)
  { return Level_id(static_cast<unsigned>(Walk::Level) - l); }

  static constexpr Level_id next_level(Level_id l)
  { return Level_id(l.get() - 1); }

  static constexpr Level_id leaf_level()
  { return Level_id(0); }

  static constexpr Level_id from_leaf_level(unsigned l)
  { return Level_id(l); }

  static constexpr unsigned lsb_for_level(Level_id level)
  { return Levels::get(level).shift(); }

  static constexpr unsigned page_order_for_level(Level_id level)
  { return Levels::get(level).shift() + Base_shift; }

  static constexpr Level_id lower_bound_level(unsigned order)
  { return Levels::lower_bound_level(order); }

  static constexpr auto get_level_desc(Level_id level)
  { return Levels::get(level); }

  template<typename FN, typename ...ARGS>
  static void for_each_level(FN &&fn, ARGS &&...args)
  {
    Traits::for_each(cxx::forward<FN>(fn), cxx::forward<ARGS>(args)...);
  }

  /**
   * Create or lookup a page table entry for a virtual address on a particular
   * page table level.
   *
   * \tparam ALLOC  Memory allocator type.
   * \tparam MEM     Memory layout type.
   * \param  virt    Virtual address for page table walk.
   * \param  level   Level in the page table hierarchy; root is at level 0.
   * \param  force_write_back  If true, `PTE_PTR::write_back()` is called on
   *                           created/changed page table entries.
   * \param  alloc   Memory allocator used for allocating new page tables.
   * \param  mem     A memory layout.
   *
   * \return Pointer to a page table entry and its level wrapped in `PTE_PTR`.
   *         If the allocation of a new page table of some level *n* fails,
   *         then the entry from level *n−1* on the page table walk is
   *         returned instead.
   *
   * During the page table walk, new page tables are created as needed using
   * `alloc`.
   */
  template< typename ALLOC, typename MEM = MEM_DFLT >
  PTE_PTR walk(Va virt, Level_id level, bool force_write_back,
               ALLOC &&alloc, MEM &&mem = MEM())
  {
    return _base.walk(ADDR::val(virt), level.get(), force_write_back,
                      cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }

  /**
   * Lookup a page table entry for a virtual address on a particular
   * page table level.
   *
   * \tparam MEM    Memory layout type.
   * \param  virt   Virtual address for page table walk.
   * \param  level  Level in the page table hierarchy; root is at level 0.
   * \param  mem    A memory layout.
   *
   * \return Pointer to a page table entry and its level wrapped in PTE_PTR.
   *         If there is no page table entry for `virt` on level `level` yet,
   *         then the last existing entry on the page table walk is returned
   *         instead.
   */
  template< typename MEM = MEM_DFLT >
  PTE_PTR walk(Va virt, Level_id level = leaf_level(), MEM &&mem = MEM()) const
  {
    return const_cast<Walk&>(_base).walk(ADDR::val(virt), level.get(), false,
                                         Null_alloc(), cxx::forward<MEM>(mem));
  }

  /**
   * Sync a range within this page table hierarchy from another
   * page table hierarchy.
   *
   * A page table hierarchy can be thought of as a tree that grows upwards:
   * - The root page table is below the first-level page tables.
   * - The second-level page tables are above the first-level page tables.
   * - ...
   *
   * After the sync all page tables above the given level are shared between
   * source and destination page table hierarchy, whereas all page tables at
   * or below the given level are allocated to each page table hierarchy
   * separately.
   *
   * Assuming a four-level page table, where level zero is the root page
   * table, and a given level of two:
   * - The third-level page tables are shared.
   * - The root, first-level and second-level page tables are not shared.
   *
   * \pre The sync range must not contain leaf pages below the given level.
   * In the case that this assumption does not apply, sync() exhibits
   * undefined behavior.
   *
   * \param l_addr The start address of the sync range in the destination
   *               page table.
   * \param _r The page table to sync from.
   * \param r_addr The start address of the sync range in the source
   *               page table.
   * \param size The size of the range to sync.
   * \param level The level to sync at.
   *
   * \retval -1 if page table allocation failed.
   * \retval  1 if a previously valid page table entry was changed
   *            during sync.
   * \retval  0 otherwise
   */
  template< typename OPTE_PTR, typename ALLOC = Null_alloc, typename MEM = MEM_DFLT >
  int sync(Va l_addr, Base<OPTE_PTR, ADDR, MEM_DFLT, TRAITS> const *_r,
           Va r_addr, Vs size, Level_id level = leaf_level(),
           bool force_write_back = false,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    Address la = ADDR::val(l_addr);
    Address ra = ADDR::val(r_addr);
    Address sz = ADDR::val(size);
    return _base.sync(la, _r->_base,
                      ra, sz, level.get(), force_write_back,
                      cxx::forward<ALLOC>(alloc),
                      cxx::forward<MEM>(mem));
  }

  /**
   * Clear all page table entries in the root page table.
   *
   * \param force_write_back  If true, `PTE_PTR::write_back()` is called on
   *                          the cleared page table entries.
   *
   * \note Page tables of non-root-level are left untouched and might get
   *       unreachable if not referenced otherwise.
   */
  void clear(bool force_write_back)
  { _base.clear(force_write_back); }

  template< typename MEM = MEM_DFLT >
  void unmap(Va virt, Vs size, Level_id level, bool force_write_back, MEM &&mem = MEM())
  {
    Address va = ADDR::val(virt);
    unsigned long sz = ADDR::val(size);
    _base.unmap(va, va + sz - 1, level.get(), force_write_back, cxx::forward<MEM>(mem));
  }

  template< typename Phys_addr, typename Attr, typename ALLOC, typename MEM = MEM_DFLT >
  [[nodiscard]]
  auto map(Phys_addr phys, Va virt, Vs size, Attr attr,
           Level_id level, bool force_write_back,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM()) -> decltype(phys - phys)
  {
    Address va = ADDR::val(virt);
    unsigned long sz = ADDR::val(size);
    return _base.map(phys, va, va + sz - 1, PTE_PTR::make_page_tmpl(level, attr),
                     level.get(), force_write_back,
                     cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }

  /**
   * Deallocate page tables.
   *
   * \tparam ALLOC       Memory allocator type.
   * \tparam MEM          Memory layout type.
   * \param  start        Begin of virtual address range (inclusive).
   * \param  end          End of virtual address range (inclusive).
   * \param  start_level  Begin of page table level range (exclusive).
   * \param  end_level    End of page table level range (inclusive).
   * \param  alloc        Memory allocator used for deallocating page tables.
   * \param  mem          A memory layout.
   *
   * Within the virtual address range from `start` to `end` (inclusive),
   * deallocate the page tables with `start_level < level <= end_level` where
   * `level` is the level of the page table in the page table hierarchy. The
   * page table entries themselves are left untouched.
   */
  template< typename ALLOC, typename MEM = MEM_DFLT >
  void destroy(Va start, Va end, Level_id start_level, Level_id end_level,
               ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    _base.destroy(ADDR::val(start), ADDR::val(end),
                  start_level.get(), end_level.get(),
                  cxx::forward<ALLOC>(alloc),
                  cxx::forward<MEM>(mem));
  }

private:
  Walk _base;
};
}
