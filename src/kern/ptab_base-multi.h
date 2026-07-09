#pragma once

#include <ptab_base.h>
#include <ptab_base-iterative.h>

namespace Ptab {

template<typename ...T>
struct Tuple;

template<typename T>
struct Tuple<T>
{
  using type = T;
  T value;

  template<unsigned IDX>
  constexpr T const &get() const { return value; }

  template<unsigned IDX>
  constexpr T &get() { return value; }
};

template<typename X, typename ...T>
struct Tuple<X, T...> : Tuple<X>, Tuple<T...>
{
  template<typename ...Y>
  constexpr Tuple(X &&x, Y &&...y) : Tuple<X>{x}, Tuple<T...>{ y... } {}

  template<unsigned IDX, typename = cxx::enable_if_t<IDX == 0>>
  constexpr typename Tuple<X>::type const &get() const { return Tuple<X>::value; }

  template<unsigned IDX, typename = cxx::enable_if_t<IDX == 0>>
  constexpr typename Tuple<X>::type &get() { return Tuple<X>::value; }

  template<unsigned IDX, typename = cxx::enable_if_t<IDX != 0>>
  constexpr auto get() const { return Tuple<T...>::template get<IDX - 1>(); }

  template<unsigned IDX, typename = cxx::enable_if_t<IDX != 0>>
  constexpr auto get() { return Tuple<T...>::template get<IDX - 1>(); }
};

template<typename ...T>
constexpr Tuple<T...> make_tuple(T &&...t)
{ return Tuple<T...>{cxx::forward<T>(t)...}; }

template< typename PTE_PTR, typename ADDR, typename MEM_DFLT,
  typename TRAITS_FOR_PASGE_SIZES,
  typename SELECT, typename T1, typename ...TRAITS>
class Multi_base
{
  using Lx = typename Level<T1>::Traits;
  using Ld = Ldesc<Lx::Base_shift, sizeof(typename PTE_PTR::Entry)>;
  using Walk = Iter_bits::Walk<PTE_PTR, typename Lx::Entry, Ld>;

  struct Pdd
  {
    Ld const *d;
    Ptab::Level_id root_level;
    unsigned root_size;
  };

  template<unsigned N> struct Lda
  {
    static constexpr Ptab::Level_id root_level{N - 1};
    unsigned root_size;
    Ld const d[N];

    constexpr operator Pdd () const { return Pdd{d, root_level, root_size}; }
  };

  template<typename T>
  static constexpr Ld ld(T const &)
  { return Ld{T::Shift, T::Size, T::May_be_leaf, true}; }

  template<typename ...T>
  static constexpr auto make_ld_array(unsigned root_size, List<T...>)
  { return Lda<sizeof...(T)>{ root_size, ld(T{})... }; }

  template<typename ...T>
  static constexpr unsigned size_ld_array(List<T...>)
  { return sizeof...(T); }

  template<typename T, typename ...X>
  static constexpr unsigned root_size(List<T, X...>)
  { return sizeof(typename T::Entry) * (1UL << T::Size); }


  static constexpr auto _pdd = make_tuple(
    make_ld_array(root_size(T1{}), T1{}),
    make_ld_array(root_size(TRAITS{}), TRAITS{})...
  );

  template<typename Y, typename ...X>
  static constexpr Pdd _get_pdd(unsigned x, Tuple<Y, X...> const &t)
  {
    if (x == 0)
      return (Pdd)t.template get<0>();
    else
      return _get_pdd<X...>(x - 1, t);
  }

  static constexpr Pdd get_pdd(unsigned x)
  {
    return _get_pdd(x, _pdd);
  }

  using Entry = typename Lx::Entry;
  Entry _e[0];

public:
  using Va = typename ADDR::Value_type;
  using Vs = typename ADDR::Value_type::Diff_type;
  using Pte_ptr = PTE_PTR;
  using Addr = ADDR;
  using Mem_default = MEM_DFLT;
  using Level_id = Ptab::Level_id;

  static constexpr Ptab::Level_id lower_bound_level(unsigned order)
  { return Ptab::Level_id(Ptab::lower_bound_level<TRAITS_FOR_PASGE_SIZES>(order)); }

  static unsigned size()
  {
    return get_pdd(SELECT::select()).root_size;
  }

  template< typename ALLOC, typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  PTE_PTR walk(Va virt, Level_id level, bool force_write_back,
               ALLOC &&alloc, MEM &&mem = MEM())
  {
    auto const &pdd = get_pdd(SELECT::select());
    return PTE_PTR(Walk::walk(_e, pdd.root_level, pdd.d,
                      ADDR::val(virt), level.get(), force_write_back,
                      cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem)));
  }

  template< typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  PTE_PTR walk(Va virt, Level_id level = leaf_level(), MEM &&mem = MEM()) const
  {
    auto const &pdd = get_pdd(SELECT::select());
    return PTE_PTR(Walk::walk(const_cast<Entry *>(_e), pdd.root_level, pdd.d,
                      ADDR::val(virt), level.get(), false,
                      Null_alloc(), cxx::forward<MEM>(mem)));
  }

  template< typename OPTE_PTR, typename ALLOC = Null_alloc, typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  int sync(Va l_addr, Multi_base<OPTE_PTR, Addr, MEM_DFLT, SELECT, T1, TRAITS...> const *_r,
           Va r_addr, Vs size, Level_id level = leaf_level(),
           bool force_write_back = false,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    auto const &pdd = get_pdd(SELECT::select());
    Address la = Addr::val(l_addr);
    Address ra = Addr::val(r_addr);
    Address sz = Addr::val(size);
    return Walk::sync(_e, pdd.root_level, pdd.d, la, _r->_e,
                      ra, sz, level.get(), force_write_back,
                      cxx::forward<ALLOC>(alloc),
                      cxx::forward<MEM>(mem));
  }

  void clear(bool force_write_back)
  FIASCO_FLATTEN
  {
    auto const &pdd = get_pdd(SELECT::select());
    Walk::clear(_e, pdd.root_level, pdd.d, force_write_back);
  }

  template< typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  void unmap(Va virt, Vs size, Level_id level, bool force_write_back, MEM &&mem = MEM())
  {
    auto const &pdd = get_pdd(SELECT::select());
    Address va = Addr::val(virt);
    unsigned long sz = Addr::val(size);
    Walk::unmap(_e, pdd.root_level, pdd.d, va, va + sz - 1, level.get(), force_write_back, cxx::forward<MEM>(mem));
  }

  template< typename Phys_addr, typename Attr, typename ALLOC, typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  [[nodiscard]]
  bool map(Phys_addr phys, Va virt, Vs size, Attr attr,
           Level_id level, bool force_write_back,
           ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    auto const &pdd = get_pdd(SELECT::select());
    Address va = Addr::val(virt);
    unsigned long sz = Addr::val(size);
    return Walk::map(_e, pdd.root_level, pdd.d, phys, va, va + sz - 1, PTE_PTR::make_page_tmpl(level, attr), level.get(),
                     force_write_back, cxx::forward<ALLOC>(alloc), cxx::forward<MEM>(mem));
  }

  template< typename ALLOC, typename MEM = MEM_DFLT >
  FIASCO_FLATTEN
  void destroy(Va start, Va end, Level_id start_level, Level_id end_level,
               ALLOC &&alloc = ALLOC(), MEM &&mem = MEM())
  {
    auto const &pdd = get_pdd(SELECT::select());
    Walk::destroy(_e, pdd.root_level, pdd.d, Addr::val(start), Addr::val(end),
                  start_level.get(), end_level.get(),
                  cxx::forward<ALLOC>(alloc),
                  cxx::forward<MEM>(mem));
  }

  static auto max_addr()
  {
    auto const &pdd = get_pdd(SELECT::select());
    // Attention: Must use 64 bit arithmetic because some page tables (namely
    // ia32 EPT) have more virtual address bits than what fits into the
    // Address type.
    return static_cast<Address>(~0ULL >> (sizeof(unsigned long long) * 8
                                         - pdd.d[pdd.root_level.get()].base_shift()
                                         - pdd.d[pdd.root_level.get()].shift()
                                         - pdd.d[pdd.root_level.get()].size()));
  }

  static Level_id root_level()
  {
    return get_pdd(SELECT::select()).root_level;
  }

  static Level_id from_root_level(unsigned l)
  {
    return Level_id(root_level().get() - l);
  }

  static constexpr Level_id next_level(Level_id l)
  { return Level_id(l.get() - 1); }

  static constexpr Level_id leaf_level()
  { return Level_id(0); }

  static constexpr Level_id from_leaf_level(unsigned l)
  { return Level_id(l); }

  static Ld get_level_desc(Level_id level)
  {
    return get_pdd(SELECT::select()).d[level.get()];
  }

  template<typename FN, typename ...ARGS>
  static void for_each_level(FN &&fn, ARGS &&...args)
  {
    auto const &pdd = get_pdd(SELECT::select());

    for (int i = pdd.root_level.get(); i >= 0; --i)
      fn(pdd.d[i], cxx::forward<ARGS>(args)...);
  }
};

}
