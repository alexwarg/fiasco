#include <dmar_space.h>

#include <task.h>
#include <ptab_base.h>
#include <bitmap.h>

#include <boot_alloc.h>
#include <intel_iommu.h>
#include <kmem_slab.h>
#include <warn.h>
#include <task_factory_impl.h>
#include <paging_bits.h>

JDB_DEFINE_TYPENAME(Dmar_space, "DMA");

Dmar_space::Dmar_pt *Dmar_space::identity_map;
bool Dmar_space::_initialized;
Dmar_space::Did_map *Dmar_space::_free_dids;
unsigned Dmar_space::_max_did;

Page_number
Dmar_space::mem_space_map_max_address() const
{
  return Page_number(1UL << (Intel::Io_mmu::hw_addr_width - Mem_space::Page_shift));
}

unsigned
Dmar_space::alloc_did()
{
  /* DID 0 may be reserved by the architecture, DID 1 is identity map. */
  for (unsigned did = 2; did < _max_did; ++did)
    if (_free_dids->atomic_get_and_set(did) == false)
      return did;

  // No DID left
  return ~0U;
}

void
Dmar_space::free_did(unsigned long did)
{
  if (_free_dids->atomic_get_and_clear(did) != true)
    panic("DMAR: Freeing free DID");
}

void
Dmar_space::init(unsigned max_did)
{
  add_page_size(Mem_space::Page_order(Config::PAGE_SHIFT));
  /* XXX CEH: Add additional page sizes based on CAP_REG[34:37] */

  _max_did = max_did;
  _free_dids = new Boot_object<Did_map>();
  _initialized = true;
}

void
Dmar_space::create_identity_map()
{
  if (identity_map)
    return;

  WARN("At least one IOMMU does not support passthrough.\n");

  check (identity_map = Kmem_alloc::allocator()->alloc_array<Dmar_pt>(1));
  identity_map->clear(false);

  Unsigned64 max_phys = 0;
  for (auto const &m: Kip::k()->mem_descs_a())
    if (m.valid() && m.type() == Mem_desc::Conventional
        && !m.is_virtual() && m.end() > max_phys)
      max_phys = m.end();

  Unsigned64 epfn;
  epfn = min(1ULL << (Intel::Io_mmu::hw_addr_width - Config::PAGE_SHIFT),
             Pg::count(max_phys + Config::PAGE_SIZE - 1));

  printf("IOMMU: identity map 0 - 0x%llx (%llu GiB)\n", Pg::size(epfn),
         Pg::size(epfn) >> 30);
  for (Unsigned64 pfn = 0; pfn <= epfn; ++pfn)
    {
      auto i = identity_map->walk(Mem_space::V_pfn(pfn),
                                  Dmar_pt::Depth, false,
                                  Kmem_alloc::q_allocator(Ram_quota::root));
      if (i.page_order() != 12)
        panic("IOMMU: cannot allocate identity IO page table, OOM\n");

      i.set((pfn << Config::PAGE_SHIFT) | 3);
    }
}

void
Dmar_space::tlb_flush_current_cpu()
{
  if (_did)
    Intel::Io_mmu::queue_and_wait_on_all_iommus(
      Intel::Io_mmu::Inv_desc::iotlb_did(_did));
}

bool
Dmar_space::v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
                     Mem_space::Page_order *order,
                     Mem_space::Attr *page_attribs)
{
  auto i = _dmarpt->walk(virt);
  // XXX CEH: Check if this hack is still needed!
  if (order)
    *order = Mem_space::Page_order(i.page_order() > 30 ? 30 : i.page_order());

  if (!i.is_valid())
    return false;

  if (phys)
    *phys = Mem_space::Phys_addr(i.page_addr());

  if (page_attribs)
    *page_attribs = i.attribs();

  return true;
}

Mem_space::Status
Dmar_space::v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
                     Mem_space::Page_order order,
                     Mem_space::Attr page_attribs)
{
  assert (cxx::is_zero(cxx::get_lsb(Mem_space::Phys_addr(phys), order)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level < Dmar_pt::Depth; ++level)
    if (Mem_space::Page_order(Dmar_pt::page_order_for_level(level)) <= order)
      break;

  auto i = _dmarpt->walk(virt, level, false,
                         Kmem_alloc::q_allocator(ram_quota()));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Mem_space::Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
      && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)))
    return Mem_space::Insert_err_exists;

  if (i.is_valid())
    {
      if (EXPECT_FALSE(!i.add_attribs(page_attribs)))
        return Mem_space::Insert_warn_exists;

      return Mem_space::Insert_warn_attrib_upgrade;
    }
  else
    {
      i.create_page(phys, page_attribs);
      return Mem_space::Insert_ok;
    }
}

L4_fpage::Rights
Dmar_space::v_delete(Mem_space::Vaddr virt, Mem_space::Page_order order,
                     L4_fpage::Rights page_attribs)
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto i = _dmarpt->walk(virt);

  if (EXPECT_FALSE(!i.is_valid()))
    return L4_fpage::Rights(0);

  if (EXPECT_FALSE(Mem_space::Page_order(i.page_order()) != order))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.access_flags();

  if (!(page_attribs & L4_fpage::Rights::R()))
    i.del_rights(page_attribs);
  else
    i.clear();

  return ret;
}

void
Dmar_space::v_set_access_flags(Mem_space::Vaddr, L4_fpage::Rights)
{}

static Mem_space::Fit_size __dmar_ps;

Mem_space::Fit_size const &
Dmar_space::mem_space_fitting_sizes() const
{ return __dmar_ps; }

void
Dmar_space::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  __dmar_ps.add_page_size(o);
}

void *
Dmar_space::operator new (size_t size, void *p) noexcept
{
  (void)size;
  assert (size == sizeof (Dmar_space));
  return p;
}

void
Dmar_space::operator delete (void *ptr) noexcept
{
  Dmar_space *t = reinterpret_cast<Dmar_space *>(ptr);
  Kmem_slab_t<Dmar_space>::q_free(t->ram_quota(), ptr);
}

void
Dmar_space::remove_from_all_iommus()
{
  unsigned long did = access_once(&_did);
  if (!did)
    return;

  // someone else changed the did
  if (!cxx::atomic_compare_exchange_strong(&_did, did, 0ul))
    return;

  bool need_wait[Intel::Io_mmu::iommus.size()];
  for (auto &mmu: Intel::Io_mmu::iommus)
    {
      need_wait[mmu.idx()] = false;

      for (unsigned bus = 0; bus < 255; ++bus)
        for (unsigned df = 0; df < 255; ++df)
          {
            auto entryp = mmu.get_context_entry(bus, df, false);
            if (!entryp)
              break; // complete bus is empty

            // There is no need for grabbing the IOMMU lock when accessing the
            // entry, since remove_from_all_iommus() is only used during the
            // destruction of a Dmar_space:
            // 1. From destroy(), which is invoked during the first destruction
            //    phase, i.e. before waiting for RCU grace period. We might miss
            //    some entries here, when entries are created for Dmar_space
            //    concurrently. In addition these entries might have a different
            //    DID, because remove_from_all_iommus() resets the DID.
            //
            // 2. From ~Dmar_space(), which is invoked during the second
            //    destruction phase, i.e. after waiting the RCU grace period.
            //    Now we clean up all the remaining context entries, if any were
            //    created since the first invocation of remove_from_all_iommus().
            Intel::Io_mmu::Cte entry = access_once(entryp.unsafe_ptr());
            // different space bound, skip
            if (entry.slptptr() != get_root(mmu.aw()))
              continue;

            // when the CAS fails someone else already unbound this slot,
            // so ignore that case
            mmu.cas_context_entry(entryp, bus, df, entry, Intel::Io_mmu::Cte(),
                                  &need_wait[mmu.idx()]);
          }
    }

  free_did(did);
  Intel::Io_mmu::queue_and_wait_on_iommus(need_wait);
}

void
Dmar_space::destroy(Kobject ***rl)
{
  Task::destroy(rl);
  remove_from_all_iommus();
}

Dmar_space::~Dmar_space()
{
  remove_from_all_iommus();

  if (_dmarpt)
    {
      _dmarpt->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Dmar_pt::Depth,
                       Kmem_alloc::q_allocator(ram_quota()));
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _dmarpt);
      _dmarpt = 0;
    }
}

namespace {

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(dmar_space_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_dma_space,
                             &Task::generic_factory<Dmar_space>);
}

}
