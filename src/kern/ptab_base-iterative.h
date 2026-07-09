#pragma once

#include <ptab_base.h>

namespace Ptab {
namespace Iter_bits {

template<typename PTE_PTR, typename ENTRY, typename LEVEL_DESC, unsigned MAX_DEPTH = 5>
class Walk
{
public:
  using Entry = ENTRY;

private:
  using Ld = LEVEL_DESC;
  using Impl = Pt_gen_level_impl<PTE_PTR, ENTRY, Ld>;
  using Self = Walk<PTE_PTR, ENTRY, Ld>;

  template< typename ALLOC >
  static Entry *alloc_next(PTE_PTR e, Level_id next_level, Ld const &desc,
                           ALLOC &&a, bool force_write_back)
  {
    Entry *n = static_cast<Entry*>(a.alloc(Bytes(sizeof(Entry) * desc.length())));
    if (EXPECT_FALSE(!n))
      return 0;

    Impl(n).clear(next_level, desc, force_write_back);
    e.set_next_level(a.to_phys(n));
    e.write_back_if(force_write_back);

    return n;
  }

public:
  using Level_desc = LEVEL_DESC;
  using Pte_ptr = PTE_PTR;
  using Template = typename PTE_PTR::Template;

  template< typename MEM >
  static void unmap(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                    Address virt_start, Address virt_end, unsigned level,
                    bool force_write_back, MEM &&mem)
  {
    if (cur_level.get() < level)
      __builtin_unreachable();

    auto const &desc = d[0];
    unsigned fs = desc.shift() + desc.size();
    if ((fs < sizeof(Address) * 8))
      {
        if (EXPECT_FALSE((virt_start >> fs) != (virt_end >> fs)))
          return;
      }

    if (level == cur_level.get())
      return Impl(n).unmap(cur_level, d[0], virt_start, virt_end, force_write_back);

    return _unmap(n, cur_level, d, virt_start, virt_end, cur_level.get() - level - 1, force_write_back,
                  cxx::forward<MEM>(mem));
  }

  template< typename Phys_addr, typename ALLOC, typename MEM >
  [[nodiscard]]
  static auto map(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                  Phys_addr phys, Address virt, Address virt_end,
                  Template tmpl, unsigned level, bool force_write_back,
                  ALLOC &&alloc, MEM &&mem) -> decltype(phys - phys)
  {
    if (cur_level.get() < level)
      __builtin_unreachable();

    auto const &desc = d[0];
    unsigned fs = desc.shift() + desc.size();
    if ((fs < sizeof(Address) * 8))
      {
        if (EXPECT_FALSE((virt >> fs) != (virt_end >> fs)))
          return decltype(phys - phys){0};
      }

    if (level == cur_level.get())
      return Impl(n).map(cur_level, d[0], phys, virt, virt_end, tmpl, force_write_back);

    return _map(n, cur_level, d, phys, virt, virt_end, tmpl,
                cur_level.get() - level - 1, force_write_back,
                cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }

  template< typename ALLOC, typename MEM >
  static void destroy(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                      Address start, Address end,
                      unsigned start_level, unsigned end_level,
                      ALLOC &&alloc, MEM &&mem)
  {
    //printf("destroy: %*.s%lx-%lx lvl=%d:%d depth=%d\n", Depth*2, "            ", start, end, start_level, end_level, Depth);
    if (end_level > start_level)
      __builtin_unreachable();

    if (!alloc.valid() || cur_level.get() <= end_level)
      return;

    _destroy(n, cur_level, d, start, end, start_level, cur_level.get() - end_level - 1,
             cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }


  template< typename ALLOC, typename MEM >
  static int sync(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                  Address &l_a, ENTRY const *_r, Address &r_a,
                  Address &size, unsigned level, bool force_write_back,
                  ALLOC &&alloc, MEM &&mem)
  {
    if (cur_level.get() < level)
      __builtin_unreachable();

    if (cur_level.get() == level)
      return Impl(n).sync(cur_level, d[0], l_a, _r, r_a, size,
                          force_write_back);

    return _sync(n, cur_level, d, l_a, _r, r_a, size, cur_level.get() - level - 1,
                 force_write_back, cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }

  static void clear(ENTRY *n, Level_id root_level, Level_desc const *d, bool force_write_back)
  { Impl(n).clear(root_level, d[0], force_write_back); }

  template< typename ALLOC, typename MEM >
  static PTE_PTR
  walk(ENTRY *n, Level_id root_level, Level_desc const *d,
       Address virt, unsigned level, bool force_write_back,
       ALLOC &&alloc, MEM &&mem)
  {
    if (root_level.get() < level || root_level.get() > MAX_DEPTH)
      __builtin_unreachable();

    for (unsigned l = root_level.get(); l >= level; --l, ++d)
      {
        PTE_PTR e = Impl(n).get_entry(Level_id(l), d[0], virt);
        if (l == level)
          return e;

        if (e.is_valid())
          {
            if (d[0].may_be_leaf() && e.is_leaf())
              return e;

            n = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(e.next_level()));
            continue;
          }

        if (!alloc.valid())
          return e;

        if (!(n = alloc_next(e, Level_id(l - 1), d[1], alloc, force_write_back)))
          return e;
      }
    __builtin_unreachable();
    return PTE_PTR();
  }

private:
  template< typename Phys_addr, typename ALLOC, typename MEM >
  [[nodiscard]]
  static auto _map(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                   Phys_addr phys, Address virt, Address virt_end,
                   Template tmpl, unsigned depth, bool force_write_back,
                   ALLOC &&alloc, MEM &&mem) -> decltype(phys - phys)
  {
    Impl _impl(n);
    auto const &desc = d[0];
    using Mapped_size = decltype(phys - phys);

    Level_id next_level(cur_level.get() - 1);
    auto virt_index_end = desc.index(virt_end);
    for (auto virt_index = desc.index(virt);;)
      {
        PTE_PTR e(n + virt_index, cur_level);
        ENTRY *x;
        if (!e.is_valid())
          {
            if (!alloc.valid() || !(x = alloc_next(e, next_level, d[1], alloc, force_write_back)))
              return Mapped_size{0};
          }
        else if (desc.may_be_leaf() && e.is_leaf())
          return Mapped_size{0};
        else
          x = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(e.next_level()));

        Address ve;
        if (virt_index < virt_index_end)
          ve = ((virt_index + 1) << desc.shift()) - 1;
        else
          ve = virt_end;

        Mapped_size mapped_size;
        if (!depth)
          mapped_size = Impl(x).map(next_level, d[1], phys, virt, ve, tmpl, force_write_back);
        else
          mapped_size = _map(x, next_level, d + 1, phys, virt, ve, tmpl, depth - 1, force_write_back,
                             alloc, mem);
        if (EXPECT_FALSE(!mapped_size))
          return mapped_size;

        if (virt_index >= virt_index_end)
          return mapped_size;

        ++virt_index;
        virt = Address{virt_index} << desc.shift();
        phys += mapped_size;
      }
  }

  template< typename MEM >
  static void _unmap(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                     Address virt_start, Address virt_end, unsigned depth,
                     bool force_write_back, MEM &&mem)
  {
    Impl _impl(n);
    auto const &desc = d[0];
    Level_id next_level(cur_level.get() - 1);
    auto virt_index_end = desc.index(virt_end);
    for (auto virt_index = desc.index(virt_start);; ++virt_index, virt_start = virt_index << desc.shift())
      {
        PTE_PTR e(n + virt_index, cur_level);
        if (!e.is_valid() || e.is_leaf())
          continue;

        Address ve;
        if (virt_index < virt_index_end)
          ve = ((virt_index + 1) << desc.shift()) - 1;
        else
          ve = virt_end;

        ENTRY *x = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(e.next_level()));
        if (!depth)
          Impl(x).unmap(next_level, d[1], virt_start, virt_end, force_write_back);
        else
          _unmap(x, next_level, d + 1, virt_start, ve, depth - 1, force_write_back, mem);

        if (virt_index >= virt_index_end)
          return;
      }
  }

  template< typename ALLOC, typename MEM >
  static void _destroy(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                       Address start, Address end,
                       unsigned start_level, unsigned depth,
                       ALLOC &&alloc, MEM &&mem)
  {
    auto const &desc = d[0];
    Impl _impl(n);

    unsigned ofs_start = desc.offset(start);
    unsigned ofs_end = desc.offset(end);
    unsigned inc = desc.offset_inc();
    //printf("destroy: %*.sofs: %d:%d\n", Depth*2, "            ", ofs_start, ofs_end);

    Level_id next_level(cur_level.get() - 1);
    for (unsigned ofs = ofs_start; ofs <= ofs_end; ofs += inc)
      {
        PTE_PTR e(Impl::entry_at(n, ofs), cur_level);
        if (!e.is_valid() || (desc.may_be_leaf() && e.is_leaf()))
          continue;

        ENTRY *x = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(e.next_level()));
        if (depth > 0)
          _destroy(x, next_level, d + 1, ofs > ofs_start ? 0 : start,
                   ofs < ofs_end ? (1UL << desc.shift())-1 : end,
                   start_level, depth - 1, alloc, mem);

        if (cur_level.get() <= start_level)
          {
            //printf("destroy: %*.sfree: %p: %p(%zd)\n", Depth*2, "            ", this, n, sizeof(Next));
            alloc.free(x, Bytes(sizeof(ENTRY) * d[1].length()));
          }
      }
  }

  template< typename ALLOC, typename MEM >
  static int _sync(ENTRY *n, Level_id cur_level, LEVEL_DESC const *d,
                  Address &l_a, ENTRY const *_r, Address &r_a,
                  Address &size, unsigned depth, bool force_write_back,
                  ALLOC &&alloc, MEM &&mem)
  {
    Impl _impl(n);
    auto const &desc = d[0];

    unsigned count = size >> desc.shift();
      {
        unsigned const lx = desc.index(l_a);
        unsigned const rx = desc.index(r_a);
        unsigned const mx = lx > rx ? lx : rx;
        if (mx + count >= desc.length())
          count = desc.length() - mx;
      }

    bool need_flush = false;

    Level_id next_level(cur_level.get() - 1);
    for (unsigned i = count; size && i > 0; --i) //while (size)
      {
        PTE_PTR l = _impl.get_entry(cur_level, desc, l_a);
        PTE_PTR r = Impl(const_cast<ENTRY *>(_r)).get_entry(cur_level, desc, r_a);
        if (!r.is_valid())
          {
            l_a += 1UL << desc.shift();
            r_a += 1UL << desc.shift();
            if (size > 1UL << desc.shift())
              {
                size -= 1UL << desc.shift();
                continue;
              }
            break;
          }

        ENTRY *x = nullptr;
        if (!l.is_valid())
          {
            if (!alloc.valid() || !(x = alloc_next(l, next_level, d[1], alloc, force_write_back)))
              return -1;
          }
        else
          x = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(l.next_level()));

        ENTRY *rn = reinterpret_cast<ENTRY *>(mem.phys_to_pmem(r.next_level()));

        int err;
        if (!depth)
          err = Impl(x).sync(next_level, d[1], l_a, rn, r_a, size, force_write_back);
        else
          err = _sync(x, next_level, d + 1, l_a, rn, r_a, size, depth - 1, force_write_back, alloc, mem);

        if (err > 0)
          need_flush = true;

        if (err < 0)
          return err;
      }

    return need_flush;
  }

};

} // namespace Iter_bits

template<typename PTE_PTR, typename ADDR, typename MEM_DFLT, typename TRAITS>
class Iterative_base;

template<typename PTE_PTR, typename ADDR, typename MEM_DFLT, typename ...TRAITS>
class Iterative_base<PTE_PTR, ADDR, MEM_DFLT, List<TRAITS...>>
{
public:
  using Va = typename ADDR::Value_type;
  using Vs = typename ADDR::Value_type::Diff_type;
  using Traits = List<TRAITS...>;
  using Pte_ptr = PTE_PTR;
  using Addr = ADDR;
  using Mem_default = MEM_DFLT;
  using L0 = typename Level<Traits>::Traits;
  using LLeaf = typename Last<Traits>::type;
  using Level_id = Ptab::Level_id;
  using Ld = Ldesc<L0::Base_shift, sizeof(typename PTE_PTR::Entry), LLeaf::Shift>;

  template<typename T>
  static constexpr Ld ld(T const &) { return Ld{T::Shift, T::Size, T::May_be_leaf, true}; }

  template<typename ...T>
  static constexpr auto make_ld_array(List<T...>) -> cxx::array<Ld, unsigned, sizeof...(T)>
  {
    return cxx::array<Ld, unsigned, sizeof...(T)>{ ld(T{})... };
  }

  static constexpr auto levels = make_ld_array(Traits{});

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
  using Entry = typename L0::Entry;
  using Walk = Ptab::Iter_bits::Walk<typename PTE_PTR::Base, Entry, Ld>;
  Entry _e[1UL << L0::Size];

  typedef Level<Traits> Levels;

public:
  static constexpr Level_id root_level()
  { return Level_id(sizeof...(TRAITS) - 1); }

  static constexpr unsigned size()
  { return sizeof(typename L0::Entry) << L0::Size; }

  static constexpr unsigned depth()
  { return root_level().get(); }

  static constexpr Level_id from_root_level(unsigned l)
  { return Level_id(root_level().get() - l); }

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

  static constexpr Ld const *ldg() { return levels.begin(); }

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
  FIASCO_FLATTEN
  PTE_PTR walk(Va virt, Level_id level, bool force_write_back,
               ALLOC &&alloc, MEM &&mem = MEM())
  {
    return PTE_PTR(Walk::walk(_e, root_level(), ldg(),
                              Addr::val(virt), level.get(), force_write_back,
                              cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem)));
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
  FIASCO_FLATTEN
  PTE_PTR walk(Va virt, Level_id level = leaf_level(), MEM &&mem = MEM()) const
  {
    return PTE_PTR(Walk::walk(const_cast<Entry *>(_e), root_level(), ldg(),
                              Addr::val(virt), level.get(), false,
                              Null_alloc(), cxx::forward<MEM>(mem)));
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
  FIASCO_FLATTEN
  int sync(Va l_addr, Iterative_base<OPTE_PTR, Addr, MEM_DFLT, Traits> const *_r,
           Va r_addr, Vs size, Level_id level = leaf_level(),
           bool force_write_back = false,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    Address la = Addr::val(l_addr);
    Address ra = Addr::val(r_addr);
    Address sz = Addr::val(size);
    return Walk::sync(_e, root_level(), ldg(), la, _r->_e,
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
  FIASCO_FLATTEN
  { Walk::clear(_e, root_level(), ldg(), force_write_back); }

  template< typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  void unmap(Va virt, Vs size, Level_id level, bool force_write_back, MEM &&mem = MEM())
  {
    Address va = Addr::val(virt);
    unsigned long sz = Addr::val(size);
    Walk::unmap(_e, root_level(), ldg(), va, va + sz - 1, level.get(), force_write_back, cxx::forward<MEM>(mem));
  }

  template< typename Phys_addr, typename ATTR, typename ALLOC, typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  [[nodiscard]]
  auto map(Phys_addr phys, Va virt, Vs size, ATTR attr,
           Level_id level, bool force_write_back,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM()) -> decltype(phys - phys)
  {
    Address va = Addr::val(virt);
    unsigned long sz = Addr::val(size);
    return Walk::map(_e, root_level(), ldg(), phys, va, va + sz - 1,
                     PTE_PTR::make_page_tmpl(level, attr), level.get(),
                     force_write_back, cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
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
  FIASCO_FLATTEN
  void destroy(Va start, Va end, Level_id start_level, Level_id end_level,
               ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    if (end_level.get() > root_level().get())
      __builtin_unreachable();

    Walk::destroy(_e, root_level(), ldg(), Addr::val(start), Addr::val(end),
                  start_level.get(), end_level.get(),
                  cxx::forward<ALLOC>(alloc),
                  cxx::forward<MEM>(mem));
  }
};
}
