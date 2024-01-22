#pragma once

#include <cassert>
#include <cpu.h>
#include <globalconfig.h>

class Context;

namespace Entry {

#ifdef CONFIG_KERNEL_ISOLATION

#define FIASCO_ASM_IRET "jmp safe_iret \n\t"

#ifdef CONFIG_INTEL_MDS_MITIGATION

inline void handle_mds_mitigations(Address *return_flags)
{
  if (*return_flags & 2)
    {
      *return_flags &= ~2UL;
      asm volatile ("verw  verw_gdt_data_kernel");
    }
}

#else // CONFIG_INTEL_MDS_MITIGATION

inline void handle_mds_mitigations(Address *)
{}

#endif // CONFIG_INTEL_MDS_MITIGATION

#ifdef CONFIG_INTEL_IA32_BRANCH_BARRIERS

inline void handle_ia32_branch_barriers(Address *return_flags)
{
  if (*return_flags & 1)
    {
      *return_flags &= ~1UL;
      Cpu::wrmsr(0, 0, 0x49);
    }
}

#else // CONFIG_INTEL_IA32_BRANCH_BARRIERS

inline void handle_ia32_branch_barriers(Address *)
{}

#endif // CONFIG_INTEL_IA32_BRANCH_BARRIERS

template<typename T>
[[noreturn]] inline void
vcpu_return_to_kernel(Context *, Mword ip, Mword sp, T arg)
{
  assert(cpu_lock.test());

  Address *p = reinterpret_cast<Address *>(Mem_layout::Kentry_cpu_page);
  handle_ia32_branch_barriers(&p[2]);
  handle_mds_mitigations(&p[2]);

  asm volatile
    ("mov %[sp], %%rsp \t\n"
     "mov %[flags], %%r11 \t\n"
     "jmp safe_sysret \t\n"
     :
     // p[0] = CPU dir pa (if PCID: + bit63 + ASID 0)
     // p[1] = KSP
     // p[2] = EXIT flags
     // p[3] = CPU dir pa + 0x1000 (if PCID: + bit63 + ASID)
     // p[4] = kernel entry scratch register
     : [cr3] "a" (p[3]),
       [flags] "i" (EFLAGS_IF), "c" (ip), [sp] "r" (sp), "D"(arg)
    );
  __builtin_unreachable();
}

#else // CONFIG_KERNEL_ISOLATION

#define FIASCO_ASM_IRET "iretq \n\t"

template<typename T>
[[noreturn]] inline void
vcpu_return_to_kernel(Context *, Mword ip, Mword sp, T arg)
{
  assert(cpu_lock.test());

  asm volatile
    ("mov %[sp], %%rsp \t\n"
     "mov %[flags], %%r11 \t\n"
     /* make RIP canonical, workaround for Intel IA32e flaw */
     "  shl     $16, %%rcx  \n"
     "  sar     $16, %%rcx  \n"
     "sysretq \t\n"
     :
     : [flags] "i" (EFLAGS_IF), "c" (ip), [sp] "r" (sp), "D"(arg)
    );
  __builtin_unreachable();
}

#endif // CONFIG_KERNEL_ISOLATION

}
