#pragma once

#include "cpu-ia32.h"
#include "asm.h"

#ifndef CONFIG_KERNEL_ISOLATION
#include "syscall_entry.h"
#include "per_cpu_array.h"
#include "tss.h"
#else
#include "mem_layout.h"
#endif // CONFIG_KERNEL_ISOLATION

class Cpu_ia32_bits : public Cpu_ia32
{
#ifndef CONFIG_KERNEL_ISOLATION
  static Per_cpu_array<Syscall_entry_data>
    _syscall_entry_data asm("syscall_entry_data");

public:
  Address volatile &kernel_sp() const
  { return *reinterpret_cast<Address volatile *>(&get_tss()->_rsp0); }

  void setup_sysenter()
  {
    extern Per_cpu_array<Syscall_entry_text> syscall_entry_text;

    wrmsr(0, GDT_CODE_KERNEL | ((GDT_CODE_USER32 | 3) << 16), MSR_STAR);
    wrmsr((Unsigned64)&syscall_entry_text[id()], MSR_LSTAR);
    wrmsr((Unsigned64)&syscall_entry_text[id()], MSR_CSTAR);
    wrmsr(~0U, MSR_SFMASK);
    _syscall_entry_data[id()].set_rsp((Address)&kernel_sp());
  }

#else // CONFIG_KERNEL_ISOLATION
public:
  void setup_sysenter() const
  {
    wrmsr(0, GDT_CODE_KERNEL | ((GDT_CODE_USER32 | 3) << 16), MSR_STAR);
    wrmsr((Unsigned64)Mem_layout::Kentry_cpu_syscall_entry, MSR_LSTAR);
    wrmsr((Unsigned64)Mem_layout::Kentry_cpu_syscall_entry, MSR_CSTAR);
    wrmsr(~0U, MSR_SFMASK);
  }

  Address volatile &kernel_sp() const
  { return *reinterpret_cast<Address volatile *>(Mem_layout::Kentry_cpu_page + sizeof(Mword)); }

#endif // CONFIG_KERNEL_ISOLATION
  static Mword stack_align(Mword stack)
  { return stack & ~0xf; }

  static bool is_canonical_address(Address addr)
  { return (addr >= (~0UL << 47)) || (addr <= (~0UL >> 17)); }

  void init_sysenter()
  {
    setup_sysenter();
    wrmsr(rdmsr(MSR_EFER) | 1, MSR_EFER);
  }

  FIASCO_CONST
  Unsigned64 ns_to_tsc(Unsigned64 ns) const
  {
    Unsigned64 tsc, dummy;
    asm inline
        ("                              \n\t"
         "mulq   %3                      \n\t"
         "shrd  $27, %%rdx, %%rax       \n\t"
         :"=a" (tsc), "=d" (dummy)
         :"a" (ns), "r" ((Unsigned64)scaler_ns_to_tsc)
        );
    return tsc;
  }

  FIASCO_CONST
  Unsigned64 tsc_to_ns(Unsigned64 tsc) const
  {
    Unsigned64 ns, dummy;
    asm inline
        ("                               \n\t"
         "mulq   %3                      \n\t"
         "shrd  $27, %%rdx, %%rax       \n\t"
         :"=a" (ns), "=d"(dummy)
         :"a" (tsc), "r" ((Unsigned64)scaler_tsc_to_ns)
        );
    return ns;
  }

  FIASCO_CONST
  Unsigned64 tsc_to_us(Unsigned64 tsc) const
  {
    Unsigned64 ns, dummy;
    asm inline
        ("                               \n\t"
         "mulq   %3                      \n\t"
         "shrd  $32, %%rdx, %%rax       \n\t"
         :"=a" (ns), "=d" (dummy)
         :"a" (tsc), "r" ((Unsigned64)scaler_tsc_to_us)
        );
    return ns;
  }

  void tsc_to_s_and_ns(Unsigned64 tsc, Unsigned32 *s, Unsigned32 *ns) const
  {
    asm inline
        ("                                \n\t"
         "mulq   %3                       \n\t"
         "shrd  $27, %%rdx, %%rax         \n\t"
         "xorq  %%rdx, %%rdx              \n\t"
         "divq  %4                        \n\t"
         :"=a" (*s), "=&d" (*ns)
         : "a" (tsc), "r" ((Unsigned64)scaler_tsc_to_ns),
           "rm"(1000000000ULL)
        );
  }

  static Unsigned64 rdtsc()
  {
    Unsigned64 h, l;
    asm inline volatile ("rdtsc" : "=a" (l), "=d" (h));
    return (h << 32) | l;
  }

  /**
   * Support for RDTSCP is indicated by CPUID.8000_0001H:EDX[27].
   */
  static Unsigned64 rdtscp()
  {
    Unsigned64 h, l;
    asm inline volatile ("rdtscp" : "=a" (l), "=d" (h) :: "rcx");
    return (h << 32) | l;
  }

  static Unsigned64 get_flags()
  {
    Unsigned64 efl;
    asm inline volatile ("pushf ; popq %0" : "=r"(efl));
    return efl;
  }

  static void set_flags(Unsigned64 efl)
  {
    asm inline volatile ("pushq %0 ; popf" : : "rm" (efl) : "memory");
  }


  static void set_cs()
  {
    // XXX have only memory indirect far jmp in 64Bit mode
    asm volatile (
    "movabsq	$1f, %%rax	\n"
    "pushq	%%rbx		\n"
    "pushq	%%rax		\n"
    "lretq 			\n"
    "1: 				\n"
      :
      : "b" (Gdt::gdt_code_kernel | Gdt::Selector_kernel)
      : "rax", "memory");
  }

  static void set_fs_gs_base(Mword *base, Mword reg)
  {
    asm volatile (
      "2: movq\t%0, %%rax\n\t"
      "   movq\t%%rax, %%rdx\n\t"
      "   shrq\t$32, %%rdx\n\t"
      "1: wrmsr\n\t"
      ".pushsection\t\".fixup.%=\", \"ax?\"\n\t"
      "3: movq\t$0, %0\n\t"
      "   jmp\t2b\n\t"
      ".popsection\n\t"
      ASM_KEX(1b, 3b)
       : "+m" (*base)
       : "c" (reg) : "rax", "rdx");
  }

  static void set_canonical_msr(Unsigned64 value, Mword reg)
  {
    asm volatile (
      "2: movq\t%%rax, %%rdx\n\t"
      "   shrq\t$32, %%rdx\n\t"
      "1: wrmsr\n\t"
      ".pushsection\t\".fixup.%=\", \"ax?\"\n\t"
      "3: movq\t$0, %%rax\n\t"
      "   jmp\t2b\n\t"
      ".popsection\n\t"
      ASM_KEX(1b, 3b)
       : "+a" (value)
       : "c" (reg) : "rdx");
  }

  static void set_fs_base(Mword *base)
  {
    set_fs_gs_base(base, MSR_FS_BASE);
  }

  static void set_gs_base(Mword *base)
  {
    set_fs_gs_base(base, MSR_GS_BASE);
  }

  void init_tss(Address tss_mem, size_t tss_size);
  void init_gdt(Address gdt_mem, Address user_max);
};

