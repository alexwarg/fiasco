#pragma once

#include <task.h>
#include <ptab_base.h>
#include <bitmap.h>

class Dmar_space :
  public cxx::Dyn_castable<Dmar_space, Task>
{
private:
  template<typename T>
  static void clean_dcache(T *p)
  { Mem_unit::clean_dcache(p, p + 1); }

  class Dmar_ptr
  {
  public:
    struct Dmar_ptr_val
    {
      Unsigned64 v;
      Dmar_ptr_val() = default;
      constexpr Dmar_ptr_val(Unsigned64 v) : v(v) {}
      CXX_BITFIELD_MEMBER(0, 1, present, v);
    };

    Dmar_ptr_val *e;

  public:
    typedef Mem_space::Attr Attr;

    Ptab::Level_id level;
    Dmar_ptr() = default;
    Dmar_ptr(Unsigned64 *e, Ptab::Level_id l)
    : e(reinterpret_cast<Dmar_ptr_val*>(e)), level(l) {}

    class Template
    {
    private:
      Dmar_ptr_val tmpl;

    public:
      Template() = default;
      constexpr Template(Dmar_ptr_val e) : tmpl(e) {}
      constexpr Dmar_ptr_val for_pa(Phys_mem_addr addr) const
      { return tmpl.v | cxx::int_value<Phys_mem_addr>(addr); }
    };

    bool is_valid() const { return e->present(); }
    bool is_leaf() const
    { return (level == Dmar_pt::leaf_level()) || (e->v & (1 << 7)); }
    Unsigned64 next_level() const
    { return cxx::mask_lsb(e->v, Config::PAGE_SHIFT); }

    void set(Unsigned64 v)
    {
      write_consistent(e, Dmar_ptr_val(v));
      clean_dcache(e);
    }

    void clear() { set(0); }

    unsigned char page_order() const
    { return Dmar_pt::page_order_for_level(level); }

    Unsigned64 page_addr() const
    {
      unsigned char o = page_order();
      return cxx::mask_lsb(e->v, o);
    }

    Attr attribs() const
    {
      typedef L4_fpage::Rights R;

      auto raw = access_once(&e->v);

      R r = R::UR();
      if (raw & 2) r |= R::W();

      return Attr::space_local(r);
    }

    bool add_attribs(Page::Attr attr)
    {
      typedef L4_fpage::Rights R;

      if (attr.rights & R::W())
        {
          auto p = access_once(&e->v);
          auto o = p;
          p |= 2;
          if (o != p)
            {
              write_now(&e->v, p);
              clean_dcache(e);
              return true;
            }
        }
      return false;
    }

    void set_next_level(Unsigned64 phys)
    { set(phys | 3); }

    void write_back_if(bool) const {}
    static void write_back(void *, void *) {}

    L4_fpage::Rights access_flags() const
    {
      return L4_fpage::Rights(0);
    }

    void del_rights(L4_fpage::Rights r)
    {
      if (r & L4_fpage::Rights::W())
        {
          auto p = access_once(&e->v);
          auto o = p & ~(Unsigned64)2;
          if (o != p)
            {
              write_now(&e->v, p);
              clean_dcache(e);
            }
        }
    }

    /*
     * WARNING: The VT-d documentation says that the super page bit
     * WARNING: is ignored in page table entries for 4k pages. However,
     * WARNING: this is not true. The super page bit must be zero.
     */
    void create_page(Phys_mem_addr addr, Page::Attr attr)
    {
      typedef L4_fpage::Rights R;

      Unsigned64 r = (level == Dmar_pt::leaf_level()) ? 0 : (Unsigned64)(1<<7);
      r |= 1; // Read
      if (attr.rights & R::W()) r |= 2;

      set(cxx::int_value<Phys_mem_addr>(addr) | r);
    }
  };

  typedef Ptab::List<Ptab::Traits<Unsigned64, 39, 9, true>,
                     Ptab::Traits<Unsigned64, 30, 9, true>,
                     Ptab::Traits<Unsigned64, 21, 9, true>,
                     Ptab::Traits<Unsigned64, 12, 9, true> > Dmar_traits;

  typedef Ptab::Shift<Dmar_traits, 12> Dmar_traits_vpn;
  typedef Ptab::Page_addr_wrap<Page_number, 12> Dmar_va_vpn;
  typedef Ptab::Base<Dmar_ptr, Dmar_va_vpn, Mem_layout, Dmar_traits_vpn> Dmar_pt;

public:
  enum { Max_nr_did = 0x10000 };

  explicit Dmar_space(Ram_quota *q)
  : Dyn_castable_class(q, Caps::mem())
  {}

  ~Dmar_space() noexcept;

  virtual void *debug_dir() const { return (void *)_dmarpt; }

  Mword get_root(int aw_level) const
  {
    return get_root(_dmarpt, aw_level);
  }

  unsigned long get_did()
  {
    // XXX: possibly need a loop here
    if (_did == 0)
      {
        unsigned ndid = alloc_did();
        if (EXPECT_FALSE(ndid == ~0U))
          return ~0UL;

        unsigned long none = 0;
        if (!cxx::atomic_compare_exchange_strong(&_did, none, (unsigned long)ndid))
          free_did(ndid);
      }
    return _did;
  }

  bool initialize()
  {
    void *b;

    if (!_initialized)
      return false;

    b = Kmem_alloc::allocator()->q_alloc(ram_quota(), Config::page_order());
    if (EXPECT_FALSE(!b))
      return false;

    _dmarpt = static_cast<Dmar_pt *>(b);
    _dmarpt->clear(false);

    /*
     * Make sure that the very first entry in a page table is valid and
     * not a super page. This is necessary if the hardware supports
     * fewer levels than the current software implementation.
     *
     * Force allocation of two levels in entry 0, so get_root works
     */
    auto i = _dmarpt->walk(Mem_space::V_pfn(0), Dmar_pt::from_root_level(2), false,
                           Kmem_alloc::q_allocator(ram_quota()));
    if (i.level != Dmar_pt::from_root_level(2))
      {
        // Got a page-table entry with the wrong level. That happens in the
        // case of an out-of-memory situation. So free everything we already
        // allocated and fail.
        _dmarpt->destroy(Virt_addr(0UL), Virt_addr(~0UL),
                         Dmar_pt::root_level(), Dmar_pt::leaf_level(),
                         Kmem_alloc::q_allocator(ram_quota()));
        Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _dmarpt);
        _dmarpt = nullptr;
        return false;
      }

    return true;
  }

  void tlb_flush_current_cpu() override;
  Page_number mem_space_map_max_address() const override;
  int resume_vcpu(Context *, Vcpu_state *, bool) override
  {
    return -L4_err::EInval;
  }

  bool
  v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
           Mem_space::Page_order *order,
           Mem_space::Attr *page_attribs) override;

  Mem_space::Status
  v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
           Mem_space::Page_order order,
           Mem_space::Attr page_attribs) override;

  L4_fpage::Rights
  v_delete(Mem_space::Vaddr virt, Mem_space::Page_order order,
           L4_fpage::Rights page_attribs) override;

  void
  v_set_access_flags(Mem_space::Vaddr, L4_fpage::Rights) override;

  Mem_space::Fit_size const &
  mem_space_fitting_sizes() const override;

  static void add_page_size(Mem_space::Page_order o);

  void *operator new (size_t size, void *p) noexcept;
  void operator delete (void *ptr) noexcept;
  void destroy(Kobject ***rl) override;

  static Mword get_root(Dmar_pt *pt, unsigned aw_level)
  {
    aw_level += 2;
    if (aw_level == Dmar_pt::depth() + 1)
      return Mem_layout::pmem_to_phys(pt);

    assert(aw_level <= Dmar_pt::depth());

    auto i = pt->walk(Mem_space::V_pfn(0), Dmar_pt::from_leaf_level(aw_level));
    assert(i.is_valid());
    return i.next_level();
  }

  static void init(unsigned max_did);

  static void create_identity_map();

  static Dmar_pt *identity_map;

private:
  Dmar_pt *_dmarpt = nullptr;
  unsigned long _did = 0;

  static bool _initialized;

  typedef Bitmap<Max_nr_did> Did_map;

  static Did_map *_free_dids;
  static unsigned _max_did;

  static unsigned alloc_did();
  static void free_did(unsigned long did);

  void remove_from_all_iommus();
};

