
#include <mem_space.h>
#include <ram_quota.h>

#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

DEFINE_PER_CPU Per_cpu<Mem_space_vz::Guest_id_alloc>
  Mem_space_vz::_guest_id_alloc(Per_cpu_data::Cpu_num);

#endif

DEFINE_PER_CPU Per_cpu<Mem_space::Asid_alloc> Mem_space::_asid_alloc;


Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs)
{
  assert (cxx::is_zero(cxx::get_lsb(phys, size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  unsigned po = cxx::int_value<Page_order>(size);
  auto i = _dir->walk(virt, po, Kmem_alloc::q_allocator(_quota));

  if (EXPECT_FALSE(i.size != po && !i.is_pte()))
    return Insert_err_nomem;

  if (EXPECT_FALSE(i.size != po)) // exists with different size
    return Insert_err_exists;

  if (EXPECT_FALSE(i.is_pte() && (i.page_addr() != phys)))
    return Insert_err_exists;

  apply_extra_page_attribs(&page_attribs);

  if (EXPECT_FALSE(i.is_pte()))
    {
      // upgrade
      page_attribs.rights |= i.rights();
      Mword x = i.make_page(phys, page_attribs);
      if (x == *i.e)
        return Insert_warn_exists;

      *i.e = x;
      // FIXME: sync TLB
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      *i.e = i.make_page(phys, page_attribs);
      // FIXME: sync TLB
      return Insert_ok;
    }
}


bool
Mem_space::v_lookup(Vaddr const virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs)
{
  auto i = _dir->walk(virt);
  if (order) *order = Page_order(i.size);

  if (!i.is_pte())
    return false;

  if (phys) *phys = i.page_addr();
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, Page_order size,
                    L4_fpage::Rights page_attribs)
{
  (void)size;
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_pte()))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.rights();

  if (! (page_attribs & L4_fpage::Rights::R()))
    {
      Mword x = i.del_rights(*i.e, page_attribs);
      if (x == *i.e)
        return ret;

      *i.e = x;
    }
  else
    i.clear();

  //i.update_tlb(Virt_addr::val(virt), c_asid(), c_vzguestid());

  return ret;
}

Mem_space::~Mem_space()
{
  reset_asid();
  reset_guest_id();
  if (_dir)
    {
      // free all page tables we have allocated for this address space
      // except the ones in kernel space which are always shared
      _dir->destroy(Virt_addr(0UL),
                    Virt_addr(Mem_layout::User_max),
                    Kmem_alloc::q_allocator(_quota));
      Kmem_alloc::allocator()->q_free(ram_quota(), Bytes(sizeof(Dir_type)), _dir);
    }
}

void
Mem_space::init()
{
  Tlb_entry::cached = Mips::Cfg<0>::read().k0() << 3;
  printf("TLB CCA: %ld\n", Tlb_entry::cached >> 3);

  init_page_sizes();
  //init_vzguestid();
}


