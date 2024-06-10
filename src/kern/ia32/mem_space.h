#pragma once

#include <mem_space_base.h>
#include <mem_space-tlb.h>
#include <mem_space-ia32-bits.h>
#include <ram_quota.h>
#include <logdefs.h>

#include <kmem_alloc.h>

#include <globalconfig.h>

#ifdef CONFIG_CPU_LOCAL_MAP
#include <mem_space-cpu-local-map.h>
template<typename M>
using Mem_space_cpu_local = Mem_space_cpu_local_map<M>;
#else
#include <mem_space-no-cpu-local-map.h>
template<typename M>
using  Mem_space_cpu_local = Mem_space_no_cpu_local_map<M>;
#endif
#ifdef CONFIG_IA32_PCID
#include <mem_space-pcid.h>
template<typename M>
using Mem_space_pcids = Mem_space_ia32_pcid<M>;
#else
#include <mem_space-no-pcid.h>
template<typename M>
using Mem_space_pcids = Mem_space_ia32_no_pcid<M>;
#endif

class Mem_space :
  public Mem_space_x<Mem_space>,
  public Mem_space_tlb<Mem_space>,
  public Mem_space_cpu_local<Mem_space>,
  public Mem_space_ia32_bits,
  public Mem_space_pcids<Mem_space>,
  public Mem_space_default_switchin<Mem_space>
{
  MEMBER_OFFSET();

public:
  // On Intel CPUs, non-present PTEs are not cached. See below for the
  // behavior on AMD CPUs.
  static constexpr bool Need_insert_tlb_flush = false;

  // On Intel CPUs, upgrading a PTE without TLB invalidation might result in
  // at most one "spurious" page-fault exception. On AMD CPUs, the page tables
  // are re-walked when any type of page fault exception is encountered by the
  // MMU to avoid the spurious page fault. On both Intel and AMD, the
  // offending TLB entry is invalidated by the CPU. TLB coherency is thus
  // eventually restored implicitly.
  static constexpr bool Need_upgrade_tlb_flush = false;

  static Address user_max()
  { return Mem_layout::User_max; }

  Mem_space(Ram_quota *q, Dir_type* pdir)
  : Mem_space_x<Mem_space>(q, pdir)
  {
    _kernel_space = this;
    _current.cpu(Cpu_number::boot_cpu()) = this;
  }

  explicit Mem_space(Ram_quota *q) : Mem_space_x<Mem_space>(q) {}

  ~Mem_space();

  static bool is_full_flush(L4_fpage::Rights rights)
  {
    return (bool)(rights & L4_fpage::Rights::R());
  }

  Address phys_dir()
  {
    return Mem_layout::pmem_to_phys(_dir);
  }

  bool set_attributes(Virt_addr virt, Attr page_attribs)
  {
    auto i = _dir->walk(virt);

    if (!i.is_valid())
      return false;

    i.set_attribs(page_attribs);
    return true;
  }

  void tlb_flush_current_cpu() FIASCO_SPACE_OVERRIDE
  {
    tlb_flush_this_();
  }

  Status v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                  Attr page_attribs) FIASCO_SPACE_OVERRIDE;
  bool v_lookup(Vaddr virt, Phys_addr *phys = 0, Page_order *order = 0,
                Attr *page_attribs = 0) FIASCO_SPACE_OVERRIDE;
  L4_fpage::Rights v_delete(Vaddr virt, Page_order size,
                            L4_fpage::Rights page_attribs) FIASCO_SPACE_OVERRIDE;
  void v_set_access_flags(Vaddr virt, L4_fpage::Rights access_flags) FIASCO_SPACE_OVERRIDE;

  /** Set this memory space as the current on this CPU. */
  void make_current(Switchin_flags flags = None)
  {
    prepare_pt_switch();
    switch_page_table(flags);
    _current.cpu(current_cpu()) = this;
  }

  using Mem_space_base::current_mem_space;
  static Mem_space *current_mem_space()
  { return _current.current(); }

  Page_order sigma0_page_size() const
  { return largest_page_size(); }

  Address virt_to_phys(Address virt) const
  {
    return dir()->virt_to_phys(virt);
  }

  Address pmem_to_phys(Address virt) const
  {
    return Mem_layout::pmem_to_phys(virt);
  }


protected:
  bool initialize()
  {
    void *b;
    if (EXPECT_FALSE(!(b = Kmem_alloc::allocator()
            ->q_alloc(_quota, Config::page_order()))))
      return false;

    _dir = static_cast<Dir_type*>(b);
    _dir->clear(false);	// initialize to zero
    return true; // success
  }

  void destroy()
  {}

private:
  void dir_shutdown();

};
