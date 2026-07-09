#pragma once

//#include <cstdio>
#include <cxx/type_traits>
#include <cxx/type_list>
#include <arithmetic.h>
#include <types.h>

namespace Ptab {

struct Null_alloc
{
  static void *alloc(Bytes) { return nullptr; }
  static void free(void *, Bytes) {}
  static bool valid() { return false; }
  static unsigned to_phys(void *) { return 0; }
};

/// index value fo a page-table level
//
// note it is undefined if a lower or higher value is of
// _l is closer to the root or to the leaf level.
class Level_id
{
public:
  Level_id() = default;
  explicit constexpr Level_id(unsigned char l) : _l(l) {}
  constexpr unsigned get() const { return _l; }
  constexpr bool operator == (Level_id rhs) const { return _l == rhs._l; }
  constexpr bool operator != (Level_id rhs) const { return _l != rhs._l; }

private:
  unsigned char _l;
};

template< typename ...T >
using List = cxx::type_list<T...>;

template<typename T>
struct Last;

template<typename T>
struct Last<List<T>> { using type = T; };

template<typename T, typename ...SFX>
struct Last<List<T, SFX...>> { using type = typename Last<List<SFX...>>::type; };


template< typename T >
struct Level;

template<typename T>
struct Level<List<T>>
{
  using Tr = List<T>;
  using Traits = T;
  enum { Id = 0 };

  static constexpr auto get(Level_id)
  { return Traits::get(); }

  static constexpr Level_id lower_bound_level(unsigned order)
  {
    constexpr unsigned o = T::Shift + T::Base_shift;
    if (o <= order)
      return Level_id(Id);
    else
      __builtin_unreachable();
  }
};

template<typename F, typename ...T>
struct Level<List<F, T...>>
{
  using Next_level = Level<List<T...>>;
  using Traits = F;
  enum { Id = Next_level::Id + 1 };

  static constexpr auto get(Level_id level)
  {
    return (level.get() == Id)
      ? Traits::get()
      : Next_level::get(level);
  }

  static constexpr Level_id lower_bound_level(unsigned order)
  {
    constexpr unsigned o = F::Shift + F::Base_shift;
    if (o <= order)
      return Level_id(Id);
    else
      return Next_level::lower_bound_level(order);
  }
};

template<typename Phys_addr>
inline cxx::enable_if_t<!cxx::is_integral_v<Phys_addr>, typename Phys_addr::Order_type>
as_order(Phys_addr, unsigned shift)
{ return typename Phys_addr::Order_type(shift); }

template<typename Phys_addr>
inline cxx::enable_if_t<cxx::is_integral_v<Phys_addr>, unsigned>
as_order(Phys_addr, unsigned shift)
{ return shift; }

template<typename Phys_addr>
inline cxx::enable_if_t<!cxx::is_integral_v<Phys_addr>, typename Phys_addr::Diff_type>
as_difference(Phys_addr a)
{ return typename Phys_addr::Diff_type(cxx::int_value<Phys_addr>(a)); }

template<typename Phys_addr>
inline cxx::enable_if_t<cxx::is_integral_v<Phys_addr>, Phys_addr>
as_difference(Phys_addr a)
{ return a; }

template<typename PTE_PTR, typename ENTRY, typename LEVEL_DESC>
class Pt_gen_level_impl
{
public:
  using Entry = ENTRY;
  using Level_desc = LEVEL_DESC;

private:
  Entry *_e;

  static constexpr unsigned idx(Level_desc const &d, Address virt)
  {
    return cxx::get_lsb(virt >> d.shift(), Address{d.size()});
  }

public:
  Pt_gen_level_impl(Entry *e) : _e(e) {}

  static Entry *entry_at(Entry *e0, unsigned offset)
  {
    return reinterpret_cast<Entry *>(reinterpret_cast<char *>(e0) + offset);
  }

  void clear(Level_id level, Level_desc const &d, bool force_write_back)
  {
    for (unsigned i = 0; i < d.length_bytes(); i += d.offset_inc())
      PTE_PTR(entry_at(_e, i), level).clear();

    if (force_write_back)
      PTE_PTR::write_back(_e, entry_at(_e, d.length_bytes()));
  }

  PTE_PTR get_entry(Level_id level, Level_desc const &d, Address virt)
  { return PTE_PTR(entry_at(_e, d.offset(virt)), level); }

  PTE_PTR get_entry(Level_id level, Level_desc const &d, Address virt) const
  { return PTE_PTR(entry_at(_e, d.offset(virt)), level); }

  void unmap(Level_id level, Level_desc const &d, Address start, Address end,
             bool force_write_back)
  {
    unsigned idx = d.offset(start);
    unsigned e = d.offset(end);
    unsigned inc = d.offset_inc();

    for (unsigned i = idx; i <= e; i += inc)
      PTE_PTR(entry_at(_e, i), level).clear();

    if (force_write_back)
      PTE_PTR::write_back(entry_at(_e, idx), entry_at(_e, e) + 1);
  }

  template<typename Phys_addr>
  auto map(Level_id level, Level_desc const &d,
           Phys_addr phys, Address virt, Address virt_end,
           typename PTE_PTR::Template tmpl, bool force_write_back) -> decltype(phys - phys)
  {
    unsigned idx = d.offset(virt);
    unsigned e = d.offset(virt_end);
    unsigned inc = d.offset_inc();

    auto phys_inc = as_difference(Phys_addr(1ULL << (d.shift() + d.base_shift())));

    Phys_addr pa = phys;
    for (unsigned i = idx; i <= e; i += inc, pa += phys_inc)
      PTE_PTR(entry_at(_e, i), level).set(tmpl.for_pa(pa));

    if (force_write_back)
      PTE_PTR::write_back(entry_at(_e, idx), entry_at(_e, e) + 1);

    return pa - phys;
  }

  int sync(Level_id level, Level_desc const &d,
           Address &l_addr, Entry const *_r, Address &r_addr,
           Address &size, bool force_write_back)
  {
    unsigned count = size >> d.shift();
    unsigned const l = d.index(l_addr);
    unsigned const r = d.index(r_addr);
    unsigned const m = l > r ? l : r;

    if (m + count >= d.length())
      count = d.length() - m;

    Entry *le = &_e[l];
    Entry const *re = &_r[r];

    bool need_flush = false;

    for (unsigned n = count; n > 0; --n)
      {
        if (PTE_PTR(&le[n-1], level).is_valid())
          need_flush = true;
#if 0
        // This loop seems unnecessary, but remote_update is also used for
        // updating the long IPC window.
        // Now consider following scenario with super pages:
        // Sender A makes long IPC to receiver B.
        // A setups the IPC window by reading the pagedir slot from B in an 
        // temporary register. Now the sender is preempted by C. Then C unmaps 
        // the corresponding super page from B. C switch to A back, using 
        // switch_to, which clears the IPC window pde slots from A. BUT then A 
        // write the  content of the temporary register, which contain the now 
        // invalid pde slot, in his own page directory and starts the long IPC.
        // Because no pagefault will happen, A will write to now invalid memory.
        // So we compare after storing the pde slot, if the copy is still
        // valid. And this solution is much faster than grabbing the cpu lock,
        // when updating the ipc window.h 
        for (;;)
          {
            typename Traits::Raw const volatile *rr
              = reinterpret_cast<typename Traits::Raw const *>(re + n - 1);
            le[n - 1] = *(Entry *)rr;
            if (EXPECT_TRUE(le[n - 1].raw() == *rr))
              break;
          }
#endif
        le[n - 1] = re[n - 1];
      }

    if (force_write_back)
      PTE_PTR::write_back(&le[0], &le[count]);

    l_addr += static_cast<unsigned long>(count) << d.shift();
    r_addr += static_cast<unsigned long>(count) << d.shift();
    size -= static_cast<unsigned long>(count) << d.shift();
    return need_flush;
  }
};

template<unsigned BASE_SHIFT, unsigned ENTRY_SIZE, unsigned MIN_SHIFT = 0>
struct Ldesc
{
  unsigned char _shift;
  unsigned char _size;
  bool _may_be_leaf:1;
  unsigned char _s_shift;
  unsigned _s_mask;

  static constexpr unsigned char entry_bits = cxx::log2u(ENTRY_SIZE);

  //bool _mask:1;

  Ldesc() = default;
  constexpr Ldesc(unsigned char shift, unsigned char size, bool may_be_leaf, bool /*mask*/)
  : _shift(shift), _size(size), _may_be_leaf(may_be_leaf),
    _s_shift(_shift - entry_bits), _s_mask(get_s_mask(size))
  {}

  constexpr unsigned shift() const { return _shift; }
  constexpr unsigned size() const { return _size; }

  constexpr bool may_be_leaf() const { return _may_be_leaf; }
  constexpr unsigned long length() const { return 1UL << _size; }

  constexpr Address index(Address addr) const
  {
    if constexpr (MIN_SHIFT < entry_bits)
      return (addr >> _shift) & _s_mask;
    else
      return ((addr >> _s_shift) & _s_mask) >> entry_bits;
  }

  constexpr Address offset(Address addr) const
  {
    if constexpr (MIN_SHIFT < entry_bits)
      return ((addr >> _shift) & _s_mask) << entry_bits;
    else
      return ((addr >> _s_shift) & _s_mask);
  }

  static constexpr Address offset_inc()
  { return 1ul << entry_bits; }

  constexpr Address length_bytes() const
  { return ENTRY_SIZE << _size; }

  static constexpr unsigned base_shift() { return BASE_SHIFT; }
  static constexpr unsigned entry_size() { return ENTRY_SIZE; }

private:
  static constexpr unsigned get_s_mask(unsigned char size)
  {
    if constexpr (MIN_SHIFT < entry_bits)
      return (1ul << size) - 1;
    else
      return ((1ul << size) - 1) << entry_bits;
  }
};

template
<
  typename _Entry,
  unsigned _Shift,
  unsigned _Size,
  bool _May_be_leaf,
  bool _Mask = true,
  unsigned _Base_shift = 0
>
struct Traits
{
  using Entry = _Entry;

  using Ld = Ldesc<_Base_shift, sizeof(_Entry)>;
  static constexpr Ld get()
  { return {_Shift, _Size, _May_be_leaf, _Mask}; }

  static constexpr unsigned Shift = _Shift;
  static constexpr unsigned Size = _Size;
  static constexpr unsigned Base_shift = _Base_shift;
  static constexpr bool May_be_leaf = _May_be_leaf;
  static constexpr bool Mask = _Mask;


  static constexpr unsigned shift() { return Shift; }
  static constexpr unsigned base_shift() { return Base_shift; }
  static constexpr unsigned size() { return Size; }
  static constexpr bool may_be_leaf() { return May_be_leaf; }
  static constexpr bool mask() { return Mask; }

  static constexpr Address index(Address addr)
  { return (addr >> Shift) & ((1UL << Size)-1); }

};

template<typename T, unsigned SHIFT>
struct Shifted_helper
{
  using type = Traits<typename T::Entry, T::Shift - SHIFT,
                      T::Size, T::May_be_leaf, T::Mask,
                      T::Base_shift + SHIFT>;
};

template<unsigned SHIFT>
struct Shifted_helper<void, SHIFT>
{
  using type = void;
};

template<typename T, unsigned SHIFT>
using Shifted = typename Shifted_helper<T, SHIFT>::type;

template< typename T, unsigned _Shift >
struct Shift_helper;

template< typename ...T, unsigned _Shift >
struct Shift_helper<List<T...>, _Shift> { using tupel = List<Shifted<T, _Shift>...>; };

template< typename T, unsigned _Shift >
using Shift = typename Shift_helper<T, _Shift>::tupel;

struct Address_wrap
{
  enum { Shift = 0 };
  typedef Address Value_type;
  static Address val(Address a) { return a; }
};

template< typename N, int SHIFT >
struct Page_addr_wrap
{
  enum { Shift = SHIFT };
  typedef N Value_type;
  static typename N::Value val(N a)
  { return cxx::int_value<N>(a); }

  static typename Value_type::Diff_type::Value
  val(typename Value_type::Diff_type a)
  { return cxx::int_value<typename Value_type::Diff_type>(a); }
};

template<typename TRAITS>
constexpr unsigned char page_order_for_level(Level_id level)
{
  using Levels = Level<TRAITS>;
  return Levels::get(level).shift() + Levels::Traits::Base_shift;
}

template<typename TRAITS>
constexpr Level_id lower_bound_level(unsigned order)
{
  return Level<TRAITS>::lower_bound_level(order);
}

}

#include <ptab_base-recursive.h>
