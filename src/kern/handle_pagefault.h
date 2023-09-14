#pragma once

#include <thread.h>
#include <mem_space.h>

namespace Page_fault
{

inline
bool handle_sigma0_page_fault(Context *c, Address pfa)
{
  Mem_space *m = c->mem_space();
  Mem_space::Page_order size = m->sigma0_page_size();
  Virt_addr va = cxx::mask_lsb(Virt_addr(pfa), size);
  return m->v_insert(Mem_space::Phys_addr(va), va, size,
                     Mem_space::Attr::space_local(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}

}

inline
int
handle_user_space_page_fault(Thread *c, Address pfa, Mword error_code)
{
  // Make sure that we do not handle page faults that do not
  // belong to this thread.
  //assert (mem_space() == current_mem_space());

  if (EXPECT_FALSE(c->mem_space()->is_sigma0()))
    {
      // special case: sigma0 can map in anything from the kernel
      if(Page_fault::handle_sigma0_page_fault(c, pfa))
        return 1;
    }

  // user mode page fault -- send pager request
  else if (c->handle_page_fault_pager(pfa, error_code,
                                      L4_msg_tag::Label_page_fault))
    return 1;

  c->do_recover_jmp_buf();
  return 0;
}


