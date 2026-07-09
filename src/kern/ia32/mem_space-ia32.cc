
#include <mem_space.h>

/** Attribute masks for page mappings. */
enum Page_attrib
{
  /// Page has been referenced
  Page_referenced = Pt_entry::Referenced,
  /// Page is dirty
  Page_dirty = Pt_entry::Dirty,
};


Mem_space::Status
Mem_space::v_insert(Phys_addr phys, Vaddr virt, Page_order size,
                    Attr page_attribs)
{
  // insert page into page table

  // XXX should modify page table using compare-and-swap

  assert (cxx::is_zero(cxx::get_lsb(Phys_addr(phys), size)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  Ptab::Level_id level = Pdir::lower_bound_level(cxx::int_value<Page_order>(size));
  auto i = _dir->walk(virt, level, false,
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
      return Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      return Insert_ok;
    }

}

void
Mem_space::v_set_access_flags(Vaddr virt, L4_fpage::Rights access_flags)
{
  auto i = _dir->walk(virt);

  if (EXPECT_FALSE(!i.is_valid()))
    return;

  unsigned page_attribs = 0;

  if (access_flags & L4_fpage::Rights::R())
    page_attribs |= Page_referenced;
  if (access_flags & L4_fpage::Rights::W())
    page_attribs |= Page_dirty;

  i.add_attribs(page_attribs);
}

/**
 * Simple page-table lookup.
 *
 * @param virt Virtual address.  This address does not need to be page-aligned.
 * @return Physical address corresponding to a.
 */
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

L4_fpage::Rights
Mem_space::v_delete(Vaddr virt, [[maybe_unused]] Page_order size,
                    L4_fpage::Rights page_attribs)
{
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), size)));

  auto i = _dir->walk(virt);

  if (EXPECT_FALSE (! i.is_valid()))
    return L4_fpage::Rights(0);

  assert (! (*i.pte & Pt_entry::global())); // Cannot unmap shared pages

  L4_fpage::Rights ret = i.access_flags();

  if (! (page_attribs & L4_fpage::Rights::R()))
    // downgrade PDE (superpage) rights
    i.del_rights(page_attribs);
  else
    // delete PDE (superpage)
    i.clear();

  return ret;
}

void
Mem_space::dir_shutdown()
{
  // free all page tables we have allocated for this address space
  // except the ones in kernel space which are always shared
  _dir->destroy(Virt_addr(0UL),
                Virt_addr(Mem_layout::User_max), Pdir::root_level(), Pdir::leaf_level(),
                Kmem_alloc::q_allocator(_quota));

  // free all unshared page table levels for the kernel space
  _dir->destroy(Virt_addr(Mem_layout::User_max + 1),
                Virt_addr(Pdir::max_addr()), Pdir::root_level(), Pdir::Super_level,
                Kmem_alloc::q_allocator(_quota));
}

Mem_space::~Mem_space()
{
  reset_asid();
  if (_dir)
    {
      dir_shutdown();
      Kmem_alloc::allocator()->q_free(_quota, Config::page_order(), _dir);
    }
}


