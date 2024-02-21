#include <intel_iommu.h>

#include <apic.h>
#include <acpi_dmar.h>
#include <cstdio>
#include <warn.h>

Intel::Io_mmu::Io_mmu_vect Intel::Io_mmu::iommus;
Acpi_dmar::Flags Intel::Io_mmu::dmar_flags;
unsigned Intel::Io_mmu::hw_addr_width;

enum { Print_infos = 0 };

bool
Intel::Io_mmu::probe(ACPI::Dmar_drhd const *drhd)
{
  base_addr = drhd->register_base;
  devs      = drhd->devs();
  segment   = drhd->segment;
  flags     = drhd->flags;

  Address va = Kmem::mmio_remap(base_addr, Config::PAGE_SIZE);

  Kip::k()->add_mem_region(Mem_desc(base_addr, base_addr + Config::PAGE_SIZE -1,
                                    Mem_desc::Reserved));
  regs.va = va;

  caps.raw = regs[Reg_64::Capabilities];
  ecaps = regs[Reg_64::Ext_capabilities];

  if (Print_infos)
    printf("IOMMU: %llx va=%lx version=%x caps=%llx:%llx\n",
           base_addr, va, regs[Reg_32::Version], caps.raw, ecaps);

  if (caps.rwbf())
    {
      WARN("IOMMU: cannot handle IOMMUs that need write-buffer flushes\n");
      return false;
    }

  if (!(ecaps & (1 << 1)))
    {
      WARN("IOMMU: queued invalidation not supported, will not use IOMMU\n");
      return false;
    }

  return true;
}

void
Intel::Io_mmu::setup(Cpu_number cpu)
{
  inv_q_size = 256 - 1;
  inv_q = Kmem_alloc::allocator()->alloc_array<Inv_desc>(inv_q_size + 1);
  Address inv_q_pa = Kmem::virt_to_phys(inv_q);

  regs[Reg_64::Inv_q_tail] = inv_q_tail * sizeof(Inv_desc);
  regs[Reg_64::Inv_q_addr] = inv_q_pa;
  modify_cmd(Cmd_qie);

  register_pm(cpu);
}

FIASCO_INIT
bool
Intel::Io_mmu::init(Cpu_number cpu)
{
  dmar_flags.raw = 0;
  auto d = Acpi::find<Acpi_dmar const *>("DMAR");
  // no DMAR -> no IRQ remapping, fall back to normal IO APIC
  if (!d)
    return false;

  dmar_flags = d->flags;
  // the value reported in the DMA remapping reporting structure is N but the
  // host address width has to be computed as (N + 1)
  hw_addr_width = d->haw + 1;

  // first count the units
  unsigned units = 0;
  for (ACPI::Dmar_head const &de: *d)
    if (de.cast<ACPI::Dmar_drhd>())
      ++units;

  if (units > Max_iommus)
    panic("Cannot handle more than %d IOMMUs", Max_iommus);

  // need to take a copy of the DMAR into the kernel AS as the ACPI
  // are mapped only into IDLE AS!
  void *dmar = Boot_alloced::alloc(d->len);
  memcpy(dmar, d, d->len);
  d = reinterpret_cast<Acpi_dmar *>(dmar);

  iommus = Io_mmu_vect(new Boot_object<Io_mmu>[units], units);

  units = 0;
  for (ACPI::Dmar_head const &de: *d)
    if (ACPI::Dmar_drhd const *i = de.cast<ACPI::Dmar_drhd>())
      {
        if (!iommus[units++].probe(i))
          {
            // Probing the IOMMU failed, not using IOMMUs.
            dmar_flags.raw = 0;
            iommus = Io_mmu_vect();
            return false;
          }

        if (i->segment != 0)
          WARN("IOMMU: no proper support for PCI Segment Groups\n");
      }

  for (auto &iommu: Intel::Io_mmu::iommus)
    iommu.setup(cpu);

  return true;
}

void
Intel::Io_mmu::allocate_root_table()
{
  Rte *root_table = Kmem_alloc::allocator()->alloc_array<Rte>(256);
  assert (root_table);
  Address root_table_pa = Kmem::virt_to_phys(root_table);
  _root_table = root_table;

  regs[Reg_64::Root_table_addr] = root_table_pa;
  modify_cmd(Cmd_srtp);
}


void
Intel::Io_mmu::pm_on_resume(Cpu_number cpu)
{
  // first enable invalidation queue
  Address inv_q_pa = Kmem::virt_to_phys(inv_q);
  inv_q_tail = 0;
  regs[Reg_64::Inv_q_tail] = inv_q_tail * sizeof(Inv_desc);
  regs[Reg_64::Inv_q_addr] = inv_q_pa;
  modify_cmd(Cmd_qie);

  if (_irq_remapping_table)
    {
      Mword target =  Apic::apic.cpu(cpu)->cpu_id();

      for (auto *irte = _irq_remapping_table;
           irte != _irq_remapping_table + (1 << _irq_remap_table_size);
           ++irte)
        {
          Intel::Io_mmu::Irte e = *irte;
          if (!e.present() || target == e.dst_xapic())
            continue;

          e.dst_xapic() = target;
          *irte = e;
        }

      Address irt_pa = Kmem::virt_to_phys(const_cast<Irte*>(_irq_remapping_table));
      regs[Reg_64::Irt_addr] = irt_pa | (_irq_remap_table_size - 1);
      modify_cmd(Cmd_sirtp);
      queue_and_wait(Inv_desc::global_iec());
      modify_cmd(Cmd_ire, Cmd_cfi);
    }

  if (_root_table)
    {
      Address root_table_pa = Kmem::virt_to_phys(_root_table);

      regs[Reg_64::Root_table_addr] = root_table_pa;
      modify_cmd(Cmd_srtp);
      queue_and_wait(Inv_desc::cc_full(), Inv_desc::iotlb_glbl());
      modify_cmd(Cmd_te);
    }
}

Intel::Io_mmu::Cte::Ptr
Intel::Io_mmu::get_context_entry(Unsigned8 bus, Unsigned8 df, bool may_alloc)
{
  cxx::Protected<Rte, true> rte = &_root_table[bus];

  if (rte->present())
    return ((Cte *)Mem_layout::phys_to_pmem(rte->ctp())) + df;

  if (EXPECT_FALSE(!may_alloc))
    return 0;

  enum { Ct_size = 4096 };
  const Bytes Ct_bytes = Bytes(Ct_size);
  void *ctx = Kmem_alloc::allocator()->alloc(Ct_bytes);
  if (EXPECT_FALSE(!ctx))
    return 0; // out of memory

  memset(ctx, 0, Ct_size);

  Rte n = Rte();
  n.present() = 1;
  n.ctp() = Mem_layout::pmem_to_phys(ctx);

    {
      auto g = lock_guard(_lock);

      if (EXPECT_FALSE(rte->present()))
        {
          // someone else allocated the context table meanwhile
          g.reset();
          // we assume context tables are never freed
          Kmem_alloc::allocator()->free(Ct_bytes, ctx);
          return ((Cte *)Mem_layout::phys_to_pmem(rte->ctp())) + df;
        }

      write_consistent(rte.unsafe_ptr(), n);
    }

  clean_dcache(rte.unsafe_ptr());

  return ((Cte *)ctx) + df;
}
