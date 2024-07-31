#pragma once

#include <cpu.h>
#include <mem_layout.h>
#include <kmem.h>
#include <globalconfig.h>

template<typename M>
class Mem_space_cpu_local_map
{
private:
  using Mem_space = M;

  Mem_space *_ths() { return static_cast<Mem_space *>(this); }

  void set_current_pcid()
  {
#ifdef CONFIG_IA32_PCID
    // [0]: CPU pdir pa + (if PCID: + bit 63 + ASID 0) -- not relevant here
    // [3]: CPU pdir pa + (if PCID: + bit 63 + ASID)
    Address pd_pa = cpu_val()[3];
    pd_pa &= ~0xfffUL;
    pd_pa |= _ths()->asid();
    cpu_val()[3] = pd_pa;
#endif
  }

  template<typename Switchin_flags>
  void set_needs_ibpb_verw(Switchin_flags flags)
  {
    (void) flags;
#if defined (CONFIG_INTEL_IA32_BRANCH_BARRIERS) || defined (CONFIG_INTEL_MDS_MITIGATION)
    // set EXIT flags CPUE_EXIT_NEED_IBPB
    Mword exit_flags = 1;
    if (!(flags & M::Vcpu_user_to_kern))
      {
        // set EXIT flags CPUE_EXIT_NEED_VERW
        exit_flags |= 2;
      }
    cpu_val()[2] |= exit_flags;
#endif
  }


protected:
  static Address *cpu_val()
  { return reinterpret_cast<Address *>(Mem_layout::Kentry_cpu_page); }

  void prepare_pt_switch()
  {
    Mword *pd = reinterpret_cast<Mword *>(Kmem::current_cpu_udir());
    Mword *d = reinterpret_cast<Mword *>(static_cast<Mem_space *>(this)->dir());
    auto *m = Kmem::pte_map();
    unsigned bit = 0;
    for (;;)
      {
        bit = m->ffs(bit);
        if (!bit)
          break;

        Mword n = d[bit - 1];
        pd[bit - 1] = n;
        if (n == 0)
          m->clear_bit(bit - 1);
        //printf("u: %u %lx\n", bit - 1, n);
        //LOG_MSG_3VAL(current(), "u", bit - 1, n, *reinterpret_cast<Mword *>(m));
      }
    Mem::barrier();
  }

  template<typename Switchin_flags>
  void switch_page_table(Switchin_flags flags)
  {
#ifdef CONFIG_KERNEL_ISOLATION
    // We are currently running on the kernel page table. Prepare for switching
    // to the user page table on kernel exit.
    set_needs_ibpb_verw(flags);
    set_current_pcid();
#else
    (void) flags;
    // switch page table directly
    Cpu::set_pdbr(access_once(&cpu_val()[0]));
#endif
  }

  int sync_kernel()
  {
    return 0;
  }
};

