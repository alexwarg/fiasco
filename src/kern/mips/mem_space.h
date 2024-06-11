#pragma once

#include <mem_space_base.h>
#include <mem_space-tlb.h>
#include <id_alloc.h>
#include <kmem.h>
#include <types.h>
#include <auto_quota.h>
#include <logdefs.h>
#include <alternatives.h>
#include <mem_unit.h>
#include <ram_quota.h>

#include <kmem_alloc.h>

#include <cstdio>
#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT
#include <mem_space-mips-vz.h>
#else // CONFIG_CPU_VIRT
struct Mem_space_vz
{
  void guest_id_init() {}
  void reset_guest_id() {}
  void set_guest_ctl1_rid(bool) {}
  void apply_extra_page_attribs(Mem_space_base::Attr *) {}
};
#endif // CONFIG_CPU_VIRT

class Mem_space :
  public Mem_space_x<Mem_space>,
  public Mem_space_tlb<Mem_space>,
  public Mem_space_vz
{
  friend class Mem_space_vz;

public:
  static constexpr bool Have_asids = true;
  static constexpr bool Need_insert_tlb_flush = false;
  static constexpr bool Need_upgrade_tlb_flush = false;

  static Address user_max()
  { return Mem_layout::User_max; }

  explicit Mem_space(Ram_quota *q)
  : Mem_space_x<Mem_space>(q)
  {
    asid(-1);
    guest_id_init();
  }

  Mem_space(Ram_quota *q, Dir_type* pdir)
  : Mem_space_x<Mem_space>(q, pdir)
  {
    asid(-1);
    guest_id_init();
    _current.cpu(Cpu_number::boot_cpu()) = this;
  }

  ~Mem_space();

  Page_order sigma0_page_size() const
  { return Virt_order(Config::SUPERPAGE_SHIFT); }


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
    add_page_size(Page_order(Config::PAGE_SHIFT));

    if (Config::have_superpages)
      add_page_size(Page_order(Config::SUPERPAGE_SHIFT));
  }

  static void kernel_space(Mem_space *_k_space)
  {
    _kernel_space = _k_space;
  }

  static void init();

  void tlb_flush_current_cpu() FIASCO_SPACE_OVERRIDE
  {
#ifdef CONFIG_CPU_VIRT
    Cpu_number cpu = current_cpu();
    Mem_unit::tlb_flush(Asid_ops::get_id(this, cpu),
                        Guest_id_ops::get_id(this, cpu));
#else
    Mem_unit::tlb_flush(c_asid(), 0);
#endif
  }


  void v_set_access_flags(Vaddr, L4_fpage::Rights) FIASCO_SPACE_OVERRIDE
  {
    // not supported currently
  }

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


  Address pmem_to_phys(Address virt) const
  {
    return Mem_layout::pmem_to_phys(virt);
  }

  Address virt_to_phys(Address virt) const
  {
    if (EXPECT_TRUE(dir() != 0))
      return dir()->virt_to_phys(virt);
    return ~0UL;
  }

  short FIASCO_PURE c_asid() const
  {
    return _asid[current_cpu()];
  }



  bool add_tlb_entry(Vaddr virt, bool write_access, bool need_probe, bool guest)
  {
    // align virt to double pages at least, as we need to add
    // two phys pages in the tlb
    Vaddr a = cxx::mask_lsb(virt, Page_order(Config::PAGE_SHIFT + 1));

    auto e = _dir->walk(virt);
    if (EXPECT_FALSE(!e.is_pte()))
      return false;

    if (EXPECT_FALSE(write_access && !e.is_writable()))
      return false;

    Mword e0, e1, pm;
    if (EXPECT_FALSE(e.size != Config::PAGE_SHIFT))
      {
        // super page, odd page sizes only supported
        assert (e.size & 1);
        e0 = e.e[0];
        e0 >>= Pdir::PWField_ptei - 2;
        e0 = (e0 & 3) << (MWORD_BITS - 2) | (e0 >> 2);
        e1 = e0 | (1UL << (e.size - 1 - Pdir::PWField_ptei));
        pm = ((1UL << e.size) - 1) & ~0x1fff;
      }
    else
      {
        // dual page mode (at leaf level)
        bool odd_page = !cxx::is_zero(virt & Vaddr(Virt_addr(1) << Page_order(Config::PAGE_SHIFT)));
        e.e -= odd_page;
        e0 = e.e[0];
        e0 >>= Pdir::PWField_ptei - 2;
        e0 = (e0 & 3) << (MWORD_BITS - 2) | (e0 >> 2);
        e1 = e.e[1];
        e1 >>= Pdir::PWField_ptei - 2;
        e1 = (e1 & 3) << (MWORD_BITS - 2) | (e1 >> 2);
        pm = ((2UL << Config::PAGE_SHIFT) - 1) & ~0x1fffUL;
        static_assert ((Config::PAGE_SHIFT & 1) == 0, "odd page sizes not supported");
      }

    Mem_unit::page_mask(pm);
    if (0)
      printf("TLB: sz=%u v=%lx p=%lx e0=%lx e1=%lx\n", e.size,
             cxx::int_value<Virt_addr>(virt), e.e[0], e0, e1);

    // assert (c_asid() == Mem_unit::entry_hi() & 0xff);
    Mword eh = cxx::int_value<Virt_addr>(a);

    if (!guest)
      eh |= c_asid();

    Mem_unit::entry_hi(eh);
    set_guest_ctl1_rid(guest);

    bool use_wr = true;
    if (need_probe)
      {
        Mips::ehb();
        Unsigned32 idx = Mem_unit::tlb_probe();
        use_wr = idx & (1UL << 31);
      }
    Mem_unit::entry_lo0(e0);
    Mem_unit::entry_lo1(e1);
    Mips::ehb();
    if (use_wr)
      asm volatile ("tlbwr");
    else
      asm volatile ("tlbwi");

    return true;
  }

  void make_current(Switchin_flags, short _asid)
  {
    // asign asid if not yet done!
    Mem_unit::set_current_asid(_asid);
    _current.current() = this;
    asm volatile (ALTERNATIVE_INSN(
          "nop",
          ASM_MTC0 " %0, $5, 5",
          (1 << 5) /* HW Page Walk */)
        : : "r"(_dir));
  }

  void make_current(Switchin_flags flags = None)
  {
    make_current(flags, asid());
  }

  void switchin_context(Mem_space *, Switchin_flags flags)
  {
#if 0
    // never switch to kernel space (context of the idle thread)
    if (this == kernel_space())
      return;
#endif

#ifdef CONFIG_CPU_VIRT
    asm volatile (ALTERNATIVE_INSN(
          "nop",
          "mtc0 %0, $10, 4",  // Load GuestCtl1 with guest ID
          0x4 /* FEATURE_VZ */)
        : : "r"(0));
#endif

    CNT_ADDR_SPACE_SWITCH;
    make_current(flags);
  }

protected:
  bool initialize()
  {
    Auto_quota<Ram_quota> q(ram_quota(), sizeof(Dir_type));
    if (EXPECT_FALSE(!q))
      return false;

    _dir = (Dir_type*)Kmem_alloc::allocator()->alloc(Bytes(sizeof(Dir_type)));
    if (!_dir)
      return false;

    _dir->clear();

    q.release();
    return true;
  }

  int sync_kernel()
  {
    return 0;
  }

private:
  // DATA

  typedef Per_cpu_array<short> Asid_array;
  Asid_array _asid;

  struct Asid_ops
  {
    enum { Id_offset = 0 };

    static bool valid(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu] >= 0; }

    static short get_id(Mem_space *o, Cpu_number cpu)
    { return o->_asid[cpu]; }

    static bool can_replace(Mem_space *v, Cpu_number cpu)
    { return v != current_mem_space(cpu); }

    static void set_id(Mem_space *o, Cpu_number cpu, short id)
    {
      write_now(&o->_asid[cpu], id);
      Mem_unit::tlb_flush(id, 0);
    }

    static void reset_id(Mem_space *o, Cpu_number cpu)
    { write_now(&o->_asid[cpu], -1); }
  };

  struct Asid_alloc : Id_alloc<unsigned char, Mem_space, Asid_ops>
  {
    Asid_alloc() : Id_alloc<unsigned char, Mem_space, Asid_ops>(256) {}
  };

  static Per_cpu<Asid_alloc> _asid_alloc;

  void asid(short a)
  {
    for (Asid_array::iterator i = _asid.begin(); i != _asid.end(); ++i)
      *i = a;
  }

  short asid()
  {
    Cpu_number cpu = current_cpu();
    return _asid_alloc.cpu(cpu).alloc(this, cpu);
  };

  void reset_asid()
  {
    for (auto i: _asid_alloc.all())
      _asid_alloc.cpu(i).free(this, i);
  }
};

