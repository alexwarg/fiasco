#pragma once

#include <globalconfig.h>
#include <initcalls.h>
#include <kip.h>
#include <mem_layout.h>
#include <paging.h>
#include <bitmap.h>
#include <simple_alloc.h>
#include <cpu.h>

#include <cstring>

class Cpu;
class Tss;

/**
 * The system's base facilities for kernel-memory management.
 * The kernel memory is a singleton object.  We access it through a
 * static class interface.
 */
class Kmem : public Mem_layout
{
  friend class Jdb;
  friend class Jdb_dbinfo;
  friend class Jdb_kern_info_misc;
  friend class Kdb;
  friend class Profile;
  friend class Vmem_alloc;
  friend class Kernel_task;

private:
  Kmem();			// default constructors are undefined
  Kmem (const Kmem&);

public:

  static Mword is_kmem_page_fault(Address addr, Mword /*error*/)
  {
    return addr > Mem_layout::User_max;
  }

  static Mword is_io_bitmap_page_fault(Address addr)
  {
    return addr >= Mem_layout::Io_bitmap &&
           addr <= Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8;
  }

  static Address kcode_start()
  { return virt_to_phys(&Mem_layout::start) & Config::PAGE_MASK; }

  static Address kcode_end()
  {
    return (virt_to_phys(&Mem_layout::end) + Config::PAGE_SIZE)
           & Config::PAGE_MASK;
  }

  static Address virt_to_phys(const void *addr)
  {
    Address a = reinterpret_cast<Address>(addr);

    if (EXPECT_TRUE(Mem_layout::in_pmem(a)))
      return Mem_layout::pmem_to_phys(a);

    if (EXPECT_TRUE(Mem_layout::in_kernel_image(a)))
      return a - Mem_layout::Kernel_image_offset;

    return kdir->virt_to_phys(a);
  }

  static const Pdir* dir()
  { return kdir; }

  static void *phys_to_virt(Address addr)
  {
    return reinterpret_cast<void *>(Mem_layout::phys_to_pmem(addr));
  }

  static Address user_max() { return ~0UL; }
  static Address io_bitmap_delimiter_page()
  {
    return reinterpret_cast<Address>(io_bitmap_delimiter);
  }

  static Address map_phys_page_tmp(Address phys, Mword idx);

  static Address kernel_image_start()
  { return virt_to_phys(&Mem_layout::image_start) & Config::PAGE_MASK; }

  static void map_phys_page(Address phys, Address virt,
                            bool cached, bool global, Address *offs = 0);

  static Address mmio_remap(Address phys, Address size, bool cache = false, bool with_exec = false);
  static void init_mmu();

#ifdef CONFIG_CPU_LOCAL_MAP
  static Kpdir *current_cpu_kdir()
  {
    return reinterpret_cast<Kpdir *>(Kentry_cpu_pdir);
  }

  static void init_cpu(Cpu &cpu);
  static void resume_cpu(Cpu_number cpu);

  static Bitmap<260> *pte_map()
  { return _pte_map; }

#ifdef CONFIG_KERNEL_ISOLATION
  static Kpdir *current_cpu_udir()
  {
    return reinterpret_cast<Kpdir *>(Kentry_cpu_pdir + 4096);
  }
#else // CONFIG_KERNEL_ISOLATION
  static Kpdir *current_cpu_udir()
  {
    return reinterpret_cast<Kpdir *>(Kentry_cpu_pdir);
  }

#endif // CONFIG_KERNEL_ISOLATION
#else // CONFIG_CPU_LOCAL_MAP
  static void init_cpu(Cpu &cpu);

  static Kpdir *current_cpu_kdir()
  {
    return kdir;
  }

  static void resume_cpu(Cpu_number)
  {
    Cpu::set_pdbr(pmem_to_phys(kdir));
  }
#endif // CONFIG_CPU_LOCAL_MAP

private:
  static Unsigned8   *io_bitmap_delimiter;
  static Address kphys_start, kphys_end;
#ifdef CONFIG_CPU_LOCAL_MAP
  static Bitmap<260> *_pte_map;
#ifdef CONFIG_KERNEL_ISOLATION
  enum { Num_cpu_dirs = 2 };

  static void setup_global_cpu_structures(bool superpages);

  static void
  setup_cpu_structures_isolation(Cpu &cpu, Kpdir *cpu_dir, cxx::Simple_alloc *cpu_m);
  static void prepare_kernel_entry_points(cxx::Simple_alloc *cpu_m, Kpdir *);

#else // CONFIG_KERNEL_ISOLATION
  enum { Num_cpu_dirs = 1 };

  static void
  setup_cpu_structures_isolation(Cpu &cpu, Kpdir *, cxx::Simple_alloc *cpu_m)
  {
    setup_cpu_structures(cpu, cpu_m, cpu_m);
  }

#endif // CONFIG_KERNEL_ISOLATION

#else // CONFIG_CPU_LOCAL_MAP
  static unsigned long tss_mem_pm;
  static cxx::Simple_alloc tss_mem_vm;

  static void setup_global_cpu_structures(bool superpages);
#endif // CONFIG_CPU_LOCAL_MAP

  static void map_initial_ram();
  static void map_kernel_virt(Kpdir *dir);
  static void setup_cpu_structures(Cpu &cpu, cxx::Simple_alloc *cpu_alloc,
                                   cxx::Simple_alloc *tss_alloc);

#ifdef CONFIG_BIT32
  static void init_cpu_arch(Cpu &cpu, cxx::Simple_alloc *cpu_mem)
  {
    // allocate the task segment for the double fault handler
    cpu.init_tss_dbf((Address)cpu_mem->alloc<Tss>(1, 0x10),
                     Mem_layout::pmem_to_phys(Kmem::dir()));

    cpu.init_sysenter();
  }

public:
  static Address
  get_realmode_startup_pdbr()
  {
    return Mem_layout::pmem_to_phys(Kmem::dir());
  }

  static Pseudo_descriptor
  get_realmode_startup_gdt_pdesc()
  {
    Gdt *_boot_gdt = Cpu::boot_cpu()->get_gdt();
    return Pseudo_descriptor(reinterpret_cast<Address>(_boot_gdt),
                             Gdt::gdt_max - 1);
  }
#endif
#ifdef CONFIG_BIT64
  static void init_cpu_arch(Cpu &, cxx::Simple_alloc *)
  {}

public:
  static Address
  get_realmode_startup_pdbr()
  {
    // for amd64 we need to make sure that our boot-up page directory is below
    // 4GB in physical memory
    static char _boot_pdir_page[Config::PAGE_SIZE] __attribute__((aligned(4096)));
    void *pd = current_cpu_kdir();
    memcpy(_boot_pdir_page, pd, sizeof(_boot_pdir_page));

    return Kmem::virt_to_phys(_boot_pdir_page);
  }

  /**
   * Get real mode startup Global Descriptor Table pseudo descriptor.
   *
   * This GDT pseudo descriptor is used for the startup code of application CPUs
   * until the proper GDT is established. To avoid issues, a copy of the
   * bootstrap CPU's GDT that is accessible via the \ref kdir mapping is
   * provided.
   *
   * \return Real mode startup Global Descriptor Table pseudo descriptor.
   */
  static Pseudo_descriptor
  get_realmode_startup_gdt_pdesc()
  {
    // For amd64, we need to make sure that our boot-up Global Descriptor Table
    // is accessible via the kdir mapping.
    static char _boot_gdt[Gdt::gdt_max] __attribute__((aligned(0x10)));

    memcpy(_boot_gdt, Cpu::boot_cpu()->get_gdt(), sizeof(_boot_gdt));
    return Pseudo_descriptor(reinterpret_cast<Address>(&_boot_gdt),
                             Gdt::gdt_max - 1);
  }

#endif

};

typedef Kmem Kmem_space;

