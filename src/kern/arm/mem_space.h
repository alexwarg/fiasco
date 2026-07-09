#pragma once

#include <mem_space_base.h>
#include <mem_space-tlb.h>
#include <mem_space-arm-bits.h>
#include <kmem_slab.h>
#include <kmem.h>

#include <globalconfig.h>

#ifdef CONFIG_ARM_V6PLUS
#include <mem_space-arm-asid.h>
using Mem_space_asid = Mem_space_arm_asid;
#else
struct Mem_space_asid
{
  static constexpr bool Have_asids = false;
  unsigned long c_asid() const
  {
    return 0;
  }
};
#endif

class Mem_space :
  public Mem_space_x<Mem_space>,
  public Mem_space_tlb<Mem_space>,
  public Mem_space_arm_bits<Mem_space>,
  public Mem_space_asid,
  public Mem_space_default_switchin<Mem_space>
{
  friend class Mem_space_arm_bits<Mem_space>;

public:
  // TLB never holds any translation table entry that generates a Translation
  // fault or an Access Flag fault.
  static constexpr bool Need_insert_tlb_flush = false;

  // TLB entry needs to be explicitly invalidated before it can be used.
  static constexpr bool Need_upgrade_tlb_flush = true;

  explicit Mem_space(Ram_quota *q) : Mem_space_x<Mem_space>(q) {}

  Mem_space(Ram_quota *q, Dir_type* pdir)
    : Mem_space_x<Mem_space>(q, pdir)
  {
    _current.cpu(Cpu_number::boot_cpu()) = this;
    _dir_phys = Phys_mem_addr(Kmem::kdir->virt_to_phys((Address)_dir));
  }

  ~Mem_space();

  static Address pmem_to_phys(Address virt)
  {
    return Mem_layout::pmem_to_phys(virt);
  }

  void tlb_flush_current_cpu() FIASCO_SPACE_OVERRIDE
  {
    if (!Have_asids)
      Mem_unit::tlb_flush();
    else if (c_asid() != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(c_asid());
  }

  void v_set_access_flags(Vaddr, L4_fpage::Rights) FIASCO_SPACE_OVERRIDE
  {}

  bool
  v_lookup(Vaddr virt, Phys_addr *phys = nullptr,
           Page_order *order = nullptr,
           Attr *page_attribs = nullptr) FIASCO_SPACE_OVERRIDE;

  Status
  v_insert(Phys_addr phys, Vaddr virt, Page_order size,
           Attr page_attribs) FIASCO_SPACE_OVERRIDE;

  L4_fpage::Rights
  v_delete(Vaddr virt, Page_order size,
           L4_fpage::Rights page_attribs) FIASCO_SPACE_OVERRIDE;

  Address virt_to_phys(Address virt) const
  {
    return dir()->virt_to_phys(virt);
  }

  Page_order sigma0_page_size() const
  {
    return Virt_order(Config::SUPERPAGE_SHIFT);
  }

  Phys_mem_addr dir_phys() const
  {
    return _dir_phys;
  }

  static void kernel_space(Mem_space *_k_space)
  {
    _kernel_space = _k_space;
  }

  static bool is_full_flush(L4_fpage::Rights rights)
  {
    return !!(rights & L4_fpage::Rights::R());
  }

  static Page_number canonize(Page_number v)
  {
    return v;
  }

  static void init_page_sizes()
  {
    init_ps(Pdir::Traits{});
  }

protected:
  bool initialize();

  template<typename ...T>
  static void init_ps(Ptab::List<T...> const &)
  {
    (add_page_size(Page_order(T::Shift + T::Base_shift)), ...);
  }


private:
  // DATA
  Phys_mem_addr _dir_phys;

  static Kmem_slab_t<Dir_type, sizeof(Dir_type)> _dir_alloc;
};
