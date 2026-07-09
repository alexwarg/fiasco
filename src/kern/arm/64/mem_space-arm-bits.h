#pragma once

#include <mem_layout.h>
#include <types.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <mem_space_base.h>
#include <cpu.h>
#include <paging.h>

template<typename M>
class Mem_space_arm_bits
{
private:
  M *_ths() { return static_cast<M *>(this); }
#ifdef CONFIG_CPU_VIRT
  static Unsigned64 _user_max;
#endif

protected:
  int sync_kernel()
  {
    return 0;
  }

  static void init_address_range()
  {
#ifdef CONFIG_CPU_VIRT
    _user_max = ~0ULL >> (64 - Page::ipa_bits(Cpu::pa_range()));
#endif
  }

public:
  void make_current(Mem_space_base::Switchin_flags = Mem_space_base::None)
  {
    asm volatile (
#ifdef CONFIG_CPU_VIRT
        "msr VTTBR_EL2, %0            \n" // set TTBR
#else
        "msr TTBR0_EL1, %0            \n" // set TTBR
#endif
        "isb                          \n"
        :
        : "r" (cxx::int_value<Phys_mem_addr>(_ths()->_dir_phys) | (_ths()->asid() << 48)));
    _ths()->_current.current() = _ths();
  }

  static Address user_max()
  {
#ifdef CONFIG_CPU_VIRT
    return _user_max;
#else
    return Mem_layout::User_max;
#endif
  }
};

#ifdef CONFIG_CPU_VIRT
template<typename M>
Unsigned64 Mem_space_arm_bits<M>::_user_max;
#endif
