
#include <mem_space.h>
#include <ram_quota.h>
#include <kmem_alloc.h>

#include <cassert>

Mem_space::~Mem_space()
{
  if (!_dir)
    return;


  // free all page tables we have allocated for this address space
  // except the ones in kernel space which are always shared
  _dir->destroy(Virt_addr(0UL),
                Virt_addr(Mem_layout::User_max), 0, Pdir::Depth,
                Kmem_alloc::q_allocator(_quota));
  // free all unshared page table levels for the kernel space
  if (Virt_addr(Mem_layout::User_max) < Virt_addr(Pdir::Max_addr))
    _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                  Virt_addr(Pdir::Max_addr), 0, Pdir::Super_level,
                  Kmem_alloc::q_allocator(_quota));
  _dir_alloc.q_free(ram_quota(), _dir);
}


bool
Mem_space::v_lookup(Vaddr virt, Phys_addr *phys,
                    Page_order *order, Attr *page_attribs)
{
  auto i = _dir->walk(virt);
  if (order) *order = Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  if (phys) *phys = Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs)
{
  bool const flush = _current.current() == this;
  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  int level;
  for (level = 0; level <= Pdir::Depth; ++level)
    if (Page_order(Pdir::page_order_for_level(level)) <= size)
      break;

  auto i = _dir->walk(virt, level, Pte_ptr::need_cache_write_back(flush),
                      Kmem_alloc::q_allocator(_quota));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
                   && (i.level != level || Phys_addr(i.page_addr()) != phys)))
    return Insert_err_exists;

  bool const valid = i.is_valid();
  if (valid)
    page_attribs.rights |= i.attribs().rights;

  auto entry = i.make_page(phys, page_attribs);

  if (valid)
    {
      if (EXPECT_FALSE(i.entry() == entry))
        return Insert_warn_exists;

      i.set_page(entry);
      i.write_back_if(flush, c_asid());
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      i.write_back_if(flush, Mem_unit::Asid_invalid);
      // Because the entry was invalid before, no TLB maintenance is necessary.
      // We still have to make sure that the MMU sees the new entry before
      // potentially triggering a page walk on it. Otherwise we might get
      // unexpected data-/prefetch-aborts...
      Mem::dsb();
      return Insert_ok;
    }
}

L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, Page_order size,
                    L4_fpage::Rights page_attribs)
{
  (void) size;
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_valid()))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.access_flags();

  if (! (page_attribs & L4_fpage::Rights::R()))
    i.del_rights(page_attribs);
  else
    i.clear();

  i.write_back_if(_current.current() == this, c_asid());

  return ret;
}


