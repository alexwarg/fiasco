#pragma once

#include <cpu.h>
#include <mem_layout.h>
#include <kmem.h>

template<typename M>
class Mem_space_no_cpu_local_map
{
private:
  using Mem_space = M;

protected:
  void prepare_pt_switch()
  {}

  template<typename Switchin_flags>
  void switch_page_table(Switchin_flags)
  {
    // switch page table directly
    Cpu::set_pdbr(Mem_layout::pmem_to_phys(static_cast<Mem_space *>(this)->dir()));
  }

  int sync_kernel()
  {
    auto *self = static_cast<Mem_space *>(this);
    return self->dir()
      ->sync(Virt_addr(Mem_layout::User_max + 1), Kmem::dir(),
             Virt_addr(Mem_layout::User_max + 1),
             Virt_size(-(Mem_layout::User_max + 1)), Pdir::Super_level,
             false,
             Kmem_alloc::q_allocator(self->ram_quota()));
  }
};

