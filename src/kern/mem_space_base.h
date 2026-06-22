#pragma once

#include <member_offs.h>
#include <types.h>
#include <paging.h>		// for page attributes
#include <mem_layout.h>
#include <cxx/cxx_int>
#include <mapdb_types.h>
#include <config.h>
#include <per_cpu_data.h>
#include <logdefs.h>

class Ram_quota;
class Mem_space;

class Mem_space_base
{
  MEMBER_OFFSET();
public:
  typedef int Status;
  static char const *const name;

  typedef Page::Attr Attr;
  typedef Pdir::Va Vaddr;
  typedef Pdir::Vs Vsize;

  typedef Addr::Addr<Config::PAGE_SHIFT> Phys_addr;
  typedef Addr::Diff<Config::PAGE_SHIFT> Phys_diff;
  typedef Addr::Order<Config::PAGE_SHIFT> Page_order;

  typedef void Reap_list;

  // for map_util
  typedef Page_number V_pfn;
  typedef Page_count V_pfc;
  typedef Addr::Order<0> V_order;

  typedef Pdir Dir_type;

  static constexpr unsigned Page_shift = Config::PAGE_SHIFT;
  static constexpr unsigned Max_num_global_page_sizes = 5;

  enum Switchin_flags
  {
    None = 0,
    Vcpu_user_to_kern = 1,
  };

  /** Return status of v_insert. */
  enum // Status
  {
    Insert_ok = 0,		///< Mapping was added successfully.
    Insert_warn_exists,		///< Mapping already existed
    Insert_warn_attrib_upgrade,	///< Mapping already existed, attribs upgrade
    Insert_err_nomem,		///< Couldn't alloc new page table
    Insert_err_exists		///< A mapping already exists at the target addr
  };

  struct Fit_size
  {
    typedef cxx::array<Page_order, Page_order, 65> Size_array;
    Size_array o;
    Page_order operator () (Page_order i) const { return o[i]; }

    void add_page_size(Page_order order)
    {
      for (Page_order c = order; c < o.size() && o[c] < order; ++c)
        o[c] = order;
    }
  };

  Mem_space_base() = default;

  explicit Mem_space_base(Ram_quota *q, Dir_type *dir = nullptr)
  : _quota(q), _dir(dir)
  {}

  Mem_space_base(Mem_space_base const &) = delete;
  Mem_space_base(Mem_space_base &&) = delete;
  void operator = (Mem_space_base const &) = delete;
  void operator = (Mem_space_base &&) = delete;


  FIASCO_SPACE_VIRTUAL
  void tlb_flush_current_cpu();

  /** Insert a page-table entry, or upgrade an existing entry with new
   *  attributes.
   *
   * @param phys  Physical address.
   * @param virt  Virtual address for which an entry should be created.
   * @param size  log2 of the page frame size.
   * @param page_attribs  Attributes for the mapping (see Page::Attr).
   * @return Insert_ok if a new mapping was created;
   *         Insert_warn_exists if the mapping already exists;
   *         Insert_warn_attrib_upgrade if the mapping already existed but
   *                                    attributes could be upgraded;
   *         Insert_err_nomem if the mapping could not be inserted because
   *                          the kernel is out of memory;
   *         Insert_err_exists if the mapping could not be inserted because
   *                           another mapping occupies the virtual-address
   *                           range
   * @pre phys and virt need to be aligned according to the size argument.
   * @pre size must match one of the frame sizes used in the page table.
   *      See fitting_sizes().
   */
  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                  Attr page_attribs);

  /** Look up a page-table entry.
   *
   * @param virt  Virtual address for which we try the lookup.
   * @param[out] phys  Meaningful only if we find something (and return true).
   *              If not 0, we fill in the physical address of the found page
   *              frame.
   * @param[out] order  If not 0, we fill in the size of the page-table slot.
   *              If an entry was found (and we return true), this is log2 of
   *              the size of the page frame.  If no entry was found (and we
   *              return false), this is the size of the free slot.  In either
   *              case, it is equal to one of the frame sizes used in the page
   *              table. See fitting_sizes().
   * @param[out] page_attribs  Meaningful only if we find something (and return
   *              true). If not 0, we fill in the page attributes for the
   *              found page frame (see Page::Attr).
   * @return True if an entry was found, false otherwise.
   */
  FIASCO_SPACE_VIRTUAL
  bool v_lookup(Vaddr virt, Phys_addr *phys = nullptr,
                Page_order *order = nullptr,
                Attr *page_attribs = nullptr);

  /** Invalidate page-table entries, or some of the entries' attributes.
   *
   * @param virt  Virtual address of the memory region that should be changed.
   * @param size  log2 size of the memory region that should be changed.
   * @param page_attribs  Revoke only the given page rights (bit-ORed, see
   *         L4_fpage::Rights). If #L4_fpage::Rights::R() is part of the
   *         bitmask, the entry is invalidated.
   *
   * @retval #L4_fpage::Rights::empty()  The entry was already invalid or the
   *         page access flags were unset before the entry was touched (by this
   *         function), or page access flags are not supported.
   * @retval otherwise  Combined (bit-ORed) page access flags of the entry
   *         before it was modified. Support for this information is
   *         platform-dependent.
   *
   * @pre `virt` needs to be aligned according to the size argument.
   * @pre `size` must match one of the frame sizes used in the page table.
   *      See fitting_sizes().
   *
   * @note No memory memory is freed.
   */
  FIASCO_SPACE_VIRTUAL
  L4_fpage::Rights v_delete(Vaddr virt, Page_order size,
                            L4_fpage::Rights page_attribs);

  /**
   * Set the page access flags on platforms where this feature is supported.
   *
   * @param virt  Virtual address of the affected memory region.
   * @param access_flags  #L4_fpage::Rights::R(): page was referenced.
   *                      #L4_fpage::Rights::W(): page is dirty.
   *
   * @note Support for setting the page access flags is platform-dependent.
   *       If this feature is not supported, this function does nothing.
   */
  FIASCO_SPACE_VIRTUAL
  void v_set_access_flags(Vaddr virt, L4_fpage::Rights access_flags);

  /**
   * Simple page-table lookup.
   *
   * This method is similar to virt_to_phys(), with the difference that this
   * version handles Sigma0's address space as a special case (by having the
   * Sigma0 task class provide a specialized override of this method): For Sigma0,
   * we do not actually consult the page table, as it is meaningless because we
   * create new mappings for Sigma0 transparently; instead, we return the
   * logically-correct result of physical address == virtual address.
   *
   * @param virt Virtual address. This address does not need to be page-aligned.
   * @return Physical address corresponding to virt.
   */
  virtual
  Address virt_to_phys_s0(void *virt) const = 0;

  virtual
  Page_number mem_space_map_max_address() const
  { return Page_number(Virt_addr(Mem_layout::User_max)) + Page_count(1); }

  virtual
  bool is_sigma0() const
  { return false; }

  Page_number map_max_address() const
  { return mem_space_map_max_address(); }

  [[gnu::pure]] FIASCO_SPACE_VIRTUAL
  Fit_size const &mem_space_fitting_sizes() const;

  Fit_size const &fitting_sizes() const
  { return mem_space_fitting_sizes(); }

  Page_order largest_page_size() const
  { return mem_space_fitting_sizes()(Page_order(64)); }

  Dir_type *dir()
  { return _dir; }

  const Dir_type *dir() const
  { return _dir; }

  Ram_quota *ram_quota() const
  { return _quota; }


  static void add_page_size(Page_order o);

  static Phys_addr page_address(Phys_addr o, Page_order s)
  { return cxx::mask_lsb(o, s); }

  static V_pfn page_address(V_pfn a, Page_order o)
  { return cxx::mask_lsb(a, o); }

  static Phys_addr subpage_address(Phys_addr addr, V_pfc offset)
  { return addr | Phys_diff(offset); }

  static Mdb_types::Pfn to_pfn(Phys_addr p)
  { return Mdb_types::Pfn(cxx::int_value<Page_number>(p)); }

  static Mdb_types::Pfn to_pfn(V_pfn p)
  { return Mdb_types::Pfn(cxx::int_value<Page_number>(p)); }

  static Mdb_types::Pcnt to_pcnt(Page_order s)
  { return Mdb_types::Pcnt(1) << Mdb_types::Order(cxx::int_value<Page_order>(s) - Config::PAGE_SHIFT); }

  static V_pfn to_virt(Mdb_types::Pfn p)
  { return Page_number(cxx::int_value<Mdb_types::Pfn>(p)); }

  static Page_order to_order(Mdb_types::Order p)
  { return Page_order(cxx::int_value<Mdb_types::Order>(p) + Config::PAGE_SHIFT); }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }

  static V_pfc subpage_offset(V_pfn a, Page_order o)
  { return cxx::get_lsb(a, o); }

  static Page_order const *get_global_page_sizes(bool finalize = true)
  {
    if (finalize)
      _glbl_page_sizes_finished = true;
    return _glbl_page_sizes;
  }

  static Mem_space *kernel_space()
  { return _kernel_space; }

  /** Return the current memory space of this CPU. */
  static Mem_space *current_mem_space(Cpu_number cpu)
  { return _current.cpu(cpu); }

protected:
  Ram_quota *_quota;
  Dir_type *_dir = nullptr;

protected:
  static void add_global_page_size(Page_order o);

  static Per_cpu<Mem_space *> _current;
  static Mem_space *_kernel_space;

  static Page_order _glbl_page_sizes[Max_num_global_page_sizes];
  static unsigned _num_glbl_page_sizes;
  static bool _glbl_page_sizes_finished;
};


template<typename M>
class Mem_space_x : public Mem_space_base
{
public:
  Mem_space_x() = default;

  explicit Mem_space_x(Ram_quota *q, Dir_type *dir = nullptr)
  : Mem_space_base(q, dir)
  {}

  Address virt_to_phys_s0(void *virt) const override
  {
    return static_cast<M const *>(this)
      ->virt_to_phys(reinterpret_cast<Address>(virt));
  }

  virtual
  bool v_fabricate(Vaddr address, Phys_addr *phys, Page_order *order,
                   Attr *attribs = nullptr)
  {
    return static_cast<M *>(this)->v_lookup(
        cxx::mask_lsb(address, Page_order(Config::PAGE_SHIFT)),
        phys, order, attribs);
  }
};

template<typename M>
struct Mem_space_default_switchin
{
  void switchin_context(M *from, Mem_space_base::Switchin_flags flags)
  {
    // FIXME: this optimization breaks SMP task deletion, an idle thread
    // may run on an already deleted page table
#if 0
    // never switch to kernel space (context of the idle thread)
    if (this == kernel_space())
      return;
#endif

    M *self = static_cast<M *>(this);
    if (from == self)
      return;

    CNT_ADDR_SPACE_SWITCH;
    self->make_current(flags);
  }
};

