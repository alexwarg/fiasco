#pragma once

#include "cpu-ia32.h"
#include "mem_layout.h"
#include "tss.h"

class Cpu_ia32_bits : public Cpu_ia32
{
  Mword _sysenter_eip;

public:
  static Mword stack_align(Mword stack)
  { return stack & ~0x3; }

  Unsigned64 ns_to_tsc(Unsigned64 ns) const
  {
    Unsigned32 dummy;
    Unsigned64 tsc;
    asm volatile
          ("movl  %%edx, %%ecx		\n\t"
           "mull	%3			\n\t"
           "movl	%%ecx, %%eax		\n\t"
           "movl	%%edx, %%ecx		\n\t"
           "mull	%3			\n\t"
           "addl	%%ecx, %%eax		\n\t"
           "adcl	$0, %%edx		\n\t"
           "shld	$5, %%eax, %%edx	\n\t"
           "shll	$5, %%eax		\n\t"
          :"=A" (tsc), "=&c" (dummy)
          : "0" (ns),  "b" (scaler_ns_to_tsc)
          );
    return tsc;
  }

  Unsigned64 tsc_to_ns(Unsigned64 tsc) const
  {
    Unsigned32 dummy1, dummy2;
    Unsigned64 ns;
    asm volatile
          ("movl  %%edx, %%ecx		\n\t"
           "mull	%4			\n\t"
           "movl  %%eax, %2		\n\t"
           "movl	%%ecx, %%eax		\n\t"
           "movl	%%edx, %%ecx		\n\t"
           "mull	%4			\n\t"
           "addl	%%ecx, %%eax		\n\t"
           "adcl	$0, %%edx		\n\t"
           "shld	$5, %%eax, %%edx	\n\t"
           "shld  $5, %2, %%eax		\n\t"
          :"=A" (ns), "=&c" (dummy1), "=&r" (dummy2)
          : "0" (tsc), "b" (scaler_tsc_to_ns)
          );
    return ns;
  }

  Unsigned64 tsc_to_us(Unsigned64 tsc) const
  {
    Unsigned32 dummy;
    Unsigned64 us;
    asm volatile
          ("movl  %%edx, %%ecx		\n\t"
           "mull	%3			\n\t"
           "movl	%%ecx, %%eax		\n\t"
           "movl	%%edx, %%ecx		\n\t"
           "mull	%3			\n\t"
           "addl	%%ecx, %%eax		\n\t"
           "adcl	$0, %%edx		\n\t"
          :"=A" (us), "=&c" (dummy)
          : "0" (tsc), "S" (scaler_tsc_to_us)
          );
    return us;
  }


  void tsc_to_s_and_ns(Unsigned64 tsc, Unsigned32 *s, Unsigned32 *ns) const
  {
      Unsigned32 dummy;
      __asm__
          ("				\n\t"
           "movl  %%edx, %%ecx		\n\t"
           "mull	%4			\n\t"
           "movl	%%ecx, %%eax		\n\t"
           "movl	%%edx, %%ecx		\n\t"
           "mull	%4			\n\t"
           "addl	%%ecx, %%eax		\n\t"
           "adcl	$0, %%edx		\n\t"
           "movl  $1000000000, %%ecx	\n\t"
           "shld	$5, %%eax, %%edx	\n\t"
           "shll	$5, %%eax		\n\t"
           "divl  %%ecx			\n\t"
          :"=a" (*s), "=d" (*ns), "=&c" (dummy)
          : "A" (tsc), "g" (scaler_tsc_to_ns)
          );
  }

  static Unsigned64 rdtsc()
  {
    Unsigned64 tsc;
    asm volatile ("rdtsc" : "=A" (tsc));
    return tsc;
  }

  static Unsigned32 get_flags()
  { Unsigned32 efl; asm volatile ("pushfl ; popl %0" : "=r"(efl)); return efl; }

  static void set_flags(Unsigned32 efl)
  { asm volatile ("pushl %0 ; popfl" : : "rm" (efl) : "memory"); }

  Address volatile &kernel_sp() const
  { return *reinterpret_cast<Address volatile *>(&get_tss()->_esp0); }

  static void set_cs()
  {
    asm volatile("ljmp %0, $1f ; 1:"
                 : : "i"(Gdt::gdt_code_kernel | Gdt::Selector_kernel));
  }

  void set_fast_entry(void (*func)(void))
  {
    if (sysenter())
      set_sysenter(func);
  }

  void setup_sysenter() const
  {
    wrmsr(Gdt::gdt_code_kernel, 0, MSR_SYSENTER_CS);
    wrmsr(reinterpret_cast<unsigned long>(&kernel_sp()), 0, MSR_SYSENTER_ESP);
    wrmsr(_sysenter_eip, 0, MSR_SYSENTER_EIP);
  }

  void init_sysenter();

  void init_tss_dbf(Address tss_dbf_mem, Address kdir);
  void init_tss(Address tss_mem, size_t tss_size);
  void init_gdt(Address gdt_mem, Address user_max);

private:
  void set_sysenter(void (*func)(void))
  {
    auto fn = reinterpret_cast<Mword>(func);
    _sysenter_eip = fn;
    wrmsr(fn, 0, MSR_SYSENTER_EIP);
  }

};
