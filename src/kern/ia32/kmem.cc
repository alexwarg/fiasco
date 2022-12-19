#include <kmem.h>
#include <kmem_alloc.h>
#include <mem_unit.h>
#include <cpu.h>
#include <tss.h>
#include <warn.h>

#include <cstring>
#include <cstdio>

enum { Print_info = 0 };

Unsigned8    *Kmem::io_bitmap_delimiter;
Address Kmem::kphys_start, Kmem::kphys_end;

// static class variables
Kpdir *Mem_layout::kdir;

// Only used for initialization and kernel debugger
Address
Kmem::map_phys_page_tmp(Address phys, Mword idx)
{
  unsigned long pte = cxx::mask_lsb(phys, Pdir::page_order_for_level(Pdir::Depth));
  Address virt;

  switch (idx)
    {
    case 0:  virt = Mem_layout::Kmem_tmp_page_1; break;
    case 1:  virt = Mem_layout::Kmem_tmp_page_2; break;
    default: return ~0UL;
    }

  static unsigned long tmp_phys_pte[2] = { ~0UL, ~0UL };

  if (pte != tmp_phys_pte[idx])
    {
      // map two consecutive pages as to be able to access
      map_phys_page(phys,          virt,          false, true);
      map_phys_page(phys + 0x1000, virt + 0x1000, false, true);
      tmp_phys_pte[idx] = pte;
    }

  return virt + phys - pte;
}

// Establish a 4k-mapping
void
Kmem::map_phys_page(Address phys, Address virt,
                    bool cached, bool global, Address *offs)
{
  auto i = kdir->walk(Virt_addr(virt), Pdir::Depth, false,
                      pdir_alloc(Kmem_alloc::allocator()));
  Mword pte = phys & Config::PAGE_MASK;

  assert(i.level == Pdir::Depth);

  i.set_page(pte, Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::Dirty
                  | (cached ? 0 : (Pt_entry::Write_through | Pt_entry::Noncacheable))
                  | (global ? Pt_entry::global() : 0));
  Mem_unit::tlb_flush_kernel(virt);

  if (offs)
    *offs = phys - pte;
}

static
bool cont_mapped(Address phys_beg, Address phys_end, Address virt)
{
  for (Address p = phys_beg, v = virt;
       p < phys_end && v < Mem_layout::Registers_map_end;
       p += Config::SUPERPAGE_SIZE, v += Config::SUPERPAGE_SIZE)
    {
      auto e = Kmem::kdir->walk(Virt_addr(v), Kmem::kdir->Super_level);
      if (!e.is_valid() || p != e.page_addr())
        return false;
    }

  return true;
}

Address
Kmem::mmio_remap(Address phys, Address size, bool cache, bool with_exec)
{
  static Address ndev = 0;
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  Address phys_end  = Mem_layout::round_superpage(phys + size);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end; a += Config::SUPERPAGE_SIZE)
    {
      if (cont_mapped(phys_page, phys_end, a))
        return (phys & ~Config::SUPERPAGE_MASK) | (a & Config::SUPERPAGE_MASK);
    }

  static_assert((Mem_layout::Registers_map_start & ~Config::SUPERPAGE_MASK) == 0,
                "Registers_map_start must be superpage-aligned");
  Address map_addr = Mem_layout::Registers_map_start + ndev;

  for (Address p = phys_page; p < phys_end; p+= Config::SUPERPAGE_SIZE)
    {
      Address dm = Mem_layout::Registers_map_start + ndev;
      assert(dm < Mem_layout::Registers_map_end);

      ndev += Config::SUPERPAGE_SIZE;

      auto m = kdir->walk(Virt_addr(dm), Pdir::Super_level);
      assert (!m.is_valid());
      assert (m.page_order() == Config::SUPERPAGE_SHIFT);
      m.set_page(m.make_page(Phys_mem_addr(p),
                             Page::Attr(with_exec ? Page::Rights::RWX() : Page::Rights::RW(),
                                        cache ? Page::Type::Normal()
                                              : Page::Type::Uncached(),
                                        Page::Kern::Global())));

      //m.write_back_if(true, Mem_unit::Asid_kernel);
      Mem_unit::tlb_flush_kernel(dm);
    }

  return (phys & ~Config::SUPERPAGE_MASK) | map_addr;
}

FIASCO_INIT
void
Kmem::map_initial_ram()
{
  Kmem_alloc *const alloc = Kmem_alloc::allocator();

  // set up the kernel mapping for physical memory.  mark all pages as
  // referenced and modified (so when touching the respective pages
  // later, we save the CPU overhead of marking the pd/pt entries like
  // this)

  // we also set up a one-to-one virt-to-phys mapping for two reasons:
  // (1) so that we switch to the new page table early and re-use the
  //     segment descriptors set up by boot_cpu.cc.  (we'll set up our
  //     own descriptors later.) we only need the first 4MB for that.
  // (2) a one-to-one phys-to-virt mapping in the kernel's page directory
  //     sometimes comes in handy (mostly useful for debugging)

  // first 4MB page
  if (!kdir->map(0, Virt_addr(0UL), Virt_size(4 << 20),
                 Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced,
                 Pt_entry::super_level(), false, pdir_alloc(alloc)))
    panic("Cannot map initial memory");
}

FIASCO_INIT_CPU
void
Kmem::map_kernel_virt(Kpdir *dir)
{
  if (!dir->map(Mem_layout::Kernel_image_phys, Virt_addr(Kernel_image),
                Virt_size(Mem_layout::Kernel_image_size),
                Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced
                | Pt_entry::global(),
                Pt_entry::super_level(), false, pdir_alloc(Kmem_alloc::allocator())))
    panic("Cannot map initial memory");
}

FIASCO_INIT
void
Kmem::init_mmu()
{
  Kmem_alloc *const alloc = Kmem_alloc::allocator();

  kdir = (Kpdir*)alloc->alloc(Config::page_order());
  memset (kdir, 0, Config::PAGE_SIZE);

  unsigned long cpu_features = Cpu::get_features();
  bool superpages = cpu_features & FEAT_PSE;

  printf("Superpages: %s\n", superpages ? "yes" : "no");

  Pt_entry::have_superpages(superpages);
  if (superpages)
    Cpu::set_cr4(Cpu::get_cr4() | CR4_PSE);

  if (cpu_features & FEAT_PGE)
    {
      Pt_entry::enable_global();
      Cpu::set_cr4 (Cpu::get_cr4() | CR4_PGE);
    }

  map_initial_ram();
  map_kernel_virt(kdir);

  bool ok = true;

  if (!Mem_layout::Adap_in_kernel_image)
    ok &= kdir->map(Mem_layout::Adap_image_phys,
                    Virt_addr(Mem_layout::Adap_image),
                    Virt_size(Config::SUPERPAGE_SIZE),
                    Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced
                    | Pt_entry::global(), Pt_entry::super_level(),
                    false, pdir_alloc(alloc));

  // map the last 64MB of physical memory as kernel memory
  ok &= kdir->map(Mem_layout::pmem_to_phys(Mem_layout::Physmem),
                  Virt_addr(Mem_layout::Physmem), Virt_size(Mem_layout::pmem_size),
                  Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::global(),
                  Pt_entry::super_level(), false, pdir_alloc(alloc));

  if (!ok)
    panic("Cannot map initial memory");

  // The service page directory entry points to an universal usable
  // page table which is currently used for the Local APIC and the
  // jdb adapter page.
  assert((Mem_layout::Service_page & ~Config::SUPERPAGE_MASK) == 0);

  kdir->walk(Virt_addr(Mem_layout::Service_page), Pdir::Depth,
             false, pdir_alloc(alloc));

  // kernel mode should acknowledge write-protected page table entries
  Cpu::set_cr0(Cpu::get_cr0() | CR0_WP);

  // now switch to our new page table
  Cpu::set_pdbr(Mem_layout::pmem_to_phys(kdir));

  setup_global_cpu_structures(superpages);


  // did we really get the first byte ??
  assert((reinterpret_cast<Address>(io_bitmap_delimiter)
          & ~Config::PAGE_MASK) == 0);
  *io_bitmap_delimiter = 0xff;
}

void
Kmem::setup_cpu_structures(Cpu &cpu, cxx::Simple_alloc *cpu_alloc,
                           cxx::Simple_alloc *tss_alloc)
{
  // now initialize the global descriptor table
  cpu.init_gdt((Address)cpu_alloc->alloc_bytes(Gdt::gdt_max, 0x10), user_max());

  // Allocate the task segment as the last thing from cpu_page_vm
  // because with IO protection enabled the task segment includes the
  // rest of the page and the following IO bitmap (2 pages).
  //
  // Allocate additional 256 bytes for emergency stack right beneath
  // the tss. It is needed if we get an NMI or debug exception at
  // entry_sys_fast_ipc/entry_sys_fast_ipc_c/entry_sys_fast_ipc_log.
  Address tss_mem = (Address)tss_alloc->alloc_bytes(sizeof(Tss) + 256, 0x10);
  assert(tss_mem + sizeof(Tss) + 256 < Mem_layout::Io_bitmap);
  tss_mem += 256;
  assert(tss_mem >= Mem_layout::Io_bitmap - 0x100000);

  // this is actually tss_size + 1, including the io_bitmap_delimiter byte
  size_t tss_size;
  tss_size = Mem_layout::Io_bitmap + (Mem_layout::Io_port_max / 8) - tss_mem;

  assert(tss_size < 0x100000); // must fit into 20 Bits

  cpu.init_tss(tss_mem, tss_size);

  // force GDT... to memory before loading the registers
  asm volatile ( "" : : : "memory" );

  // set up the x86 CPU's memory model
  cpu.set_gdt();
  cpu.set_ldt(0);

  cpu.set_ds(Gdt::data_segment());
  cpu.set_es(Gdt::data_segment());
  cpu.set_ss(Gdt::gdt_data_kernel | Gdt::Selector_kernel);
  cpu.set_fs(Gdt::gdt_data_user   | Gdt::Selector_user);
  cpu.set_gs(Gdt::gdt_data_user   | Gdt::Selector_user);
  cpu.set_cs();

  // and finally initialize the TSS
  cpu.set_tss();

  init_cpu_arch(cpu, cpu_alloc);
}

#ifdef CONFIG_CPU_LOCAL_MAP
Bitmap<260> *Kmem::_pte_map;

DEFINE_PER_CPU static Per_cpu<Kpdir *> _per_cpu_dir;

void
Kmem::resume_cpu(Cpu_number cpu)
{
  Cpu::set_pdbr(pmem_to_phys(_per_cpu_dir.cpu(cpu)));
}

FIASCO_INIT_CPU
void
Kmem::init_cpu(Cpu &cpu)
{
  Kmem_alloc *const alloc = Kmem_alloc::allocator();

  unsigned const cpu_dir_sz = sizeof(Kpdir) * Num_cpu_dirs;

  Kpdir *cpu_dir = (Kpdir*)alloc->alloc(Bytes(cpu_dir_sz));
  memset (cpu_dir, 0, cpu_dir_sz);

  auto src = kdir->walk(Virt_addr(0), 0);
  auto dst = cpu_dir->walk(Virt_addr(0), 0);
  write_now(dst.pte, *src.pte);

  static_assert ((Kglobal_area & ((1UL << 30) - 1)) == 0, "Kglobal area must be 1GB aligned");
  static_assert ((Kglobal_area_end & ((1UL << 30) - 1)) == 0, "Kglobal area must be 1GB aligned");

  for (unsigned i = 0; i < ((Kglobal_area_end - Kglobal_area) >> 30); ++i)
    {
      auto src = kdir->walk(Virt_addr(Kglobal_area + (((Address)i) << 30)), 1);
      auto dst = cpu_dir->walk(Virt_addr(Kglobal_area + (((Address)i) << 30)), 1,
                               false, pdir_alloc(alloc));

      if (dst.level != 1)
        panic("could not setup per-cpu page table: %d\n", __LINE__);

      if (src.level != 1)
        panic("could not setup per-cpu page table, invalid source mapping: %d\n", __LINE__);

      write_now(dst.pte, *src.pte);
    }

  static_assert((Physmem & (Config::SUPERPAGE_SIZE - 1)) == 0, "Physmem area must be superpage aligned");
  static_assert((Physmem_end& (Config::SUPERPAGE_SIZE - 1)) == 0, "Physmem_end area must be superpage aligned");

  for (unsigned i = 0; i < ((Physmem_end - Physmem) >> Config::SUPERPAGE_SHIFT);)
    {
      Address a = Physmem + (i << Config::SUPERPAGE_SHIFT);
      if ((a & ((1UL << 30) - 1)) || ((Physmem_end - (1UL << 30)) < a))
        {
          // copy a superpage slot
          auto src = kdir->walk(Virt_addr(a), 2);

          if (src.level != 2)
            panic("could not setup per-cpu page table, invalid source mapping: %d\n", __LINE__);

          if (src.is_valid())
            {
              auto dst = cpu_dir->walk(Virt_addr(a), 2,
                                       false, pdir_alloc(alloc));

              if (dst.level != 2)
                panic("could not setup per-cpu page table: %d\n", __LINE__);

              if (dst.is_valid())
                {
                  assert (*dst.pte == *src.pte);
                  ++i;
                  continue;
                }

              if (Print_info)
                printf("physmem sync(2M): va:%16lx pte:%16lx\n", a, *src.pte);

              write_now(dst.pte, *src.pte);
            }
          ++i;
        }
      else
        {
          // copy a 1GB slot
          auto src = kdir->walk(Virt_addr(a), 1);
          if (src.level != 1)
            panic("could not setup per-cpu page table, invalid source mapping: %d\n", __LINE__);

          if (src.is_valid())
            {
              auto dst = cpu_dir->walk(Virt_addr(a), 1,
                                       false, pdir_alloc(alloc));

              if (dst.level != 1)
                panic("could not setup per-cpu page table: %d\n", __LINE__);

              if (dst.is_valid())
                {
                  assert (*dst.pte == *src.pte);
                  i += 512; // skip 512 2MB entries == 1G
                  continue;
                }

              if (Print_info)
                printf("physmem sync(1G): va:%16lx pte:%16lx\n", a, *src.pte);

              write_now(dst.pte, *src.pte);
            }

          i += 512; // skip 512 2MB entries == 1G
        }
    }

  map_kernel_virt(cpu_dir);

  bool ok = true;

  if (!Adap_in_kernel_image)
    ok &= cpu_dir->map(Adap_image_phys,
                       Virt_addr(Adap_image),
                       Virt_size(Config::SUPERPAGE_SIZE),
                       Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced
                       | Pt_entry::global(), Pt_entry::super_level(),
                       false, pdir_alloc(alloc));

  Address cpu_dir_pa = Mem_layout::pmem_to_phys(cpu_dir);
  ok &= cpu_dir->map(cpu_dir_pa, Virt_addr(Kentry_cpu_pdir),
                     Virt_size(cpu_dir_sz),
                     Pt_entry::Writable | Pt_entry::Referenced
                     | Pt_entry::Dirty  | Pt_entry::global(),
                     Pdir::Depth,
                     false, pdir_alloc(alloc));

  unsigned const cpu_mx_sz = Config::PAGE_SIZE;
  void *cpu_mx = alloc->alloc(Bytes(cpu_mx_sz));
  auto cpu_mx_pa = Mem_layout::pmem_to_phys(cpu_mx);

  ok &= cpu_dir->map(cpu_mx_pa, Virt_addr(Kentry_cpu_page), Virt_size(cpu_mx_sz),
                     Pt_entry::Writable | Pt_entry::Referenced
                     | Pt_entry::Dirty  | Pt_entry::global(),
                     Pdir::Depth,
                     false, pdir_alloc(alloc));

  if (!ok)
    panic("Cannot map initial CPU memory");

  _per_cpu_dir.cpu(cpu.id()) = cpu_dir;
  Cpu::set_pdbr(cpu_dir_pa);

  cxx::Simple_alloc cpu_m(Kentry_cpu_page, Config::PAGE_SIZE);
  // [0] = CPU dir pa (PCID: + bit63 + ASID 0)
  // [1] = KSP
  // [2] = EXIT flags
  // [3] = CPU dir pa + 0x1000 (PCID: + bit63 + ASID)
  // [4] = entry scratch register
  // [5] = unused
  // [6] = here starts the syscall entry code
  Mword *p = cpu_m.alloc<Mword>(6);
  // With PCID enabled set bit 63 to prevent flushing of any TLB entries or
  // paging-structure caches during the page table switch. In that case TLB
  // flushes are exclusively done by Mem_unit::tlb_flush() calls.
  Mword const flush_tlb_bit = Config::Pcid_enabled ? 1UL << 63 : 0;
  write_now(&p[0], cpu_dir_pa | flush_tlb_bit);
  write_now(&p[3], cpu_dir_pa | flush_tlb_bit |  0x1000);
  setup_cpu_structures_isolation(cpu, cpu_dir, &cpu_m);

  auto *pte_map = cpu_m.alloc<Bitmap<260> >(1, 0x20);

  pte_map->clear_all();
  // Sync pte_map bits for context switch optimization.
  // Slots > 255 are CPU local / kernel area.
  for (unsigned long i = 0; i < 256; ++i)
    if (cpu_dir->walk(Virt_addr(i << 39), 0).is_valid())
      pte_map->set_bit(i);

  if (!_pte_map)
    _pte_map = pte_map;
  else if (_pte_map != pte_map)
    panic("failed to allocate correct PTE map: %p expected %p\n",
          pte_map, _pte_map);
}

#ifdef CONFIG_KERNEL_ISOLATION

void
Kmem::setup_global_cpu_structures(bool superpages)
{
  (void)superpages;
  io_bitmap_delimiter = (Unsigned8 *)Kmem_alloc::allocator()
                                        ->alloc(Config::page_order());
}

void
Kmem::setup_cpu_structures_isolation(Cpu &cpu, Kpdir *cpu_dir, cxx::Simple_alloc *cpu_m)
{

  auto src = cpu_dir->walk(Virt_addr(Kentry_cpu_page), 0);
  auto dst = cpu_dir[1].walk(Virt_addr(Kentry_cpu_page), 0);
  write_now(dst.pte, *src.pte);

  // map kernel code to user space dir
  extern char _kernel_text_start[];
  extern char _kernel_text_entry_end[];
  Address ki_page = ((Address)_kernel_text_start) & ~(Config::PAGE_SIZE - 1);
  Address kie_page = (((Address)_kernel_text_entry_end) + (Config::PAGE_SIZE - 1)) & ~(Config::PAGE_SIZE - 1);

  if (Print_info)
    printf("kernel code: %p(%lx)-%p(%lx)\n", _kernel_text_start,
           ki_page, _kernel_text_entry_end, kie_page);

  // FIXME: Make sure we can and do share level 1 to 3 among all CPUs
  if (!cpu_dir[1].map(ki_page - Kernel_image_offset, Virt_addr(ki_page),
                      Virt_size(kie_page - ki_page),
                      Pt_entry::Referenced | Pt_entry::global(),
                      Pdir::Depth,
                      false, pdir_alloc(Kmem_alloc::allocator())))
    panic("Cannot map initial memory");

  prepare_kernel_entry_points(cpu_m, cpu_dir);

  unsigned const estack_sz = 512;
  char *estack = (char *)cpu_m->alloc_bytes(estack_sz, 16);

  setup_cpu_structures(cpu, cpu_m, cpu_m);
  cpu.get_tss()->_rsp0 = (Address)(estack + estack_sz);
}

void
Kmem::prepare_kernel_entry_points(cxx::Simple_alloc *cpu_m, Kpdir *)
{
  extern char const syscall_entry_code[];
  extern char const syscall_entry_code_end[];
  char *sccode = (char *)cpu_m->alloc_bytes(syscall_entry_code_end - syscall_entry_code, 16);
  assert ((Address)sccode == Kentry_cpu_syscall_entry);
  memcpy(sccode, syscall_entry_code, syscall_entry_code_end - syscall_entry_code);
}
#endif // CONFIG_KERNEL_ISOLATION

#else // CONFIG_CPU_LOCAL_MAP

unsigned long Kmem::tss_mem_pm;
cxx::Simple_alloc Kmem::tss_mem_vm;

void
Kmem::setup_global_cpu_structures(bool superpages)
{
  auto *alloc = Kmem_alloc::allocator();
  assert((Mem_layout::Io_bitmap & ~Config::SUPERPAGE_MASK) == 0);

  enum { Tss_mem_size = 0x10 + Config::Max_num_cpus * cxx::ceil_lsb(sizeof(Tss) + 256, 4) };

  /* Per-CPU TSS required to use IO-bitmap for more CPUs */
  static_assert(Tss_mem_size < 0x10000, "Too many CPUs configured.");

  unsigned tss_mem_size = Tss_mem_size;

  if (tss_mem_size < Config::PAGE_SIZE)
    tss_mem_size = Config::PAGE_SIZE;

  tss_mem_pm = Mem_layout::pmem_to_phys(alloc->alloc(Bytes(tss_mem_size)));

  printf("Kmem:: TSS mem at %lx (%uBytes)\n", tss_mem_pm, tss_mem_size);

  if (superpages
      && Config::SUPERPAGE_SIZE - (tss_mem_pm & ~Config::SUPERPAGE_MASK) < 0x10000)
    {
      // can map as 4MB page because the cpu_page will land within a
      // 16-bit range from io_bitmap
      auto e = kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE),
                          Pdir::Super_level, false, pdir_alloc(alloc));

      e.set_page(tss_mem_pm & Config::SUPERPAGE_MASK,
                 Pt_entry::Writable | Pt_entry::Referenced
                 | Pt_entry::Dirty | Pt_entry::global());

      tss_mem_vm = cxx::Simple_alloc(
          (tss_mem_pm & ~Config::SUPERPAGE_MASK)
          + (Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE),
          tss_mem_size);
    }
  else
    {
      unsigned i;
      for (i = 0; (i << Config::PAGE_SHIFT) < tss_mem_size; ++i)
        {
          auto e = kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::PAGE_SIZE * (i+1)),
                              Pdir::Depth, false, pdir_alloc(alloc));

          e.set_page(tss_mem_pm + i * Config::PAGE_SIZE,
                     Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::Dirty
                     | Pt_entry::global());
        }

      tss_mem_vm = cxx::Simple_alloc(
          Mem_layout::Io_bitmap - Config::PAGE_SIZE * i,
          tss_mem_size);
    }

  // the IO bitmap must be followed by one byte containing 0xff
  // if this byte is not present, then one gets page faults
  // (or general protection) when accessing the last port
  // at least on a Pentium 133.
  //
  // Therefore we write 0xff in the first byte of the cpu_page
  // and map this page behind every IO bitmap
  io_bitmap_delimiter = tss_mem_vm.alloc<Unsigned8>();
}

FIASCO_INIT_CPU
void
Kmem::init_cpu(Cpu &cpu)
{
  cxx::Simple_alloc cpu_mem_vm(Kmem_alloc::allocator()->alloc(Bytes(1024)), 1024);
  if (Warn::is_enabled(Info))
    printf("Allocate cpu_mem @ %p\n", cpu_mem_vm.block());

  // now switch to our new page table
  Cpu::set_pdbr(Mem_layout::pmem_to_phys(kdir));

  setup_cpu_structures(cpu, &cpu_mem_vm, &tss_mem_vm);
}


#endif

