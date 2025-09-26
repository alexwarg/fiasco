#pragma once

#include <bootstrap-arm-types.h>
#include <globalconfig.h>
#include <paging-page.h>
#include <mem.h>
#include <mem.h>
#include <mem_layout.h>
#include <cpu.h>
#include <kip.h>
#include <config.h>

extern char start_of_loader[];
extern char end_of_loader[];
namespace Bootstrap
{
inline void set_mair0(Mword v)
{
  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(v));
}

inline void disable_mmu_and_caches()
{
  unsigned long sctlr;
  asm ("mrc p15, 0, %0, c1, c0" : "=r" (sctlr));
  if (sctlr & Cpu::Cp15_c1_mmu)
    {
      if (sctlr & Cpu::Cp15_c1_cache)
        {
          Mmu<>::clean_dcache(&bs_info, &bs_info + 1);
          Mmu<>::clean_dcache(start_of_loader, end_of_loader);
        }

      sctlr &= ~(Cpu::Cp15_c1_mmu | Cpu::Cp15_c1_cache | Cpu::Cp15_c1_insn_cache);
      asm volatile("mcr p15, 0, %0, c1, c0" : : "r" (sctlr));
      Mem::isb();
      Mem::barrier();
    }
}

#ifdef CONFIG_ARM_V6PLUS

inline void set_asid()
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r" (0)); // ASID 0
}

inline void set_ttbcr()
{
  asm volatile("mcr p15, 0, %[ttbcr], c2, c0, 2" // TTBCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
}

#else
inline void set_asid() {}
inline void set_ttbcr() {}
#endif

#ifdef CONFIG_CPU_VIRT

inline void create_initial_mappings()
{
  typedef Bootstrap::Phys_addr Phys_addr;
  typedef Bootstrap::Virt_addr Virt_addr;

  void *page_dir = kern_to_boot(bs_info.pi.kernel_page_directory);

  // Map whole RAM 1:1. This is needed by Fiasco because of
  // Mem_op::arm_mem_cache_maint() needing to have access to potentially all
  // RAM. Assumes that no platform ever has physical RAM above the lowest
  // virtual address in the Mem_layout!
  Kip *kip = reinterpret_cast<Kip*>(kern_to_boot(bs_info.kip));
  for (auto const &md: kip->mem_descs_a())
    {
      if (!md.valid())
        {
          const_cast<Mem_desc&>(md).type(Mem_desc::Undefined);
          continue;
        }

      if (md.is_virtual())
        continue;

      if (md.type() == Mem_desc::Conventional)
        {
          auto va = Virt_addr(md.start());
          auto pa = Phys_addr(md.start());
          for (; va < Virt_addr(md.end());
               va += Bootstrap::map_page_size, pa += Bootstrap::map_page_size_phys)
            Bootstrap::map_memory(page_dir, va, pa, true, false);
        }
    }

  Virt_addr va;
  Phys_addr pa;

  // map kernel to desired virtual address
  for (va = Virt_addr(Mem_layout::Map_base),
       pa = Phys_addr(Super_pg::trunc(bs_info.kernel_start_phys));
       pa < Phys_addr(Super_pg::ceil(bs_info.kernel_end_phys));
       va += Bootstrap::map_page_size, pa += Bootstrap::map_page_size_phys)
    Bootstrap::map_memory(page_dir, va, pa, true, false);
}

#else // CONFIG_CPU_VIRT

static void switch_to_el1()
{
  Mword cpsr;
  asm volatile ("mrs %0, cpsr" : "=r"(cpsr));
  if ((cpsr & 0x1f) != 0x13)
    { // we have no hyp mode support so leave hyp mode
      Mword tmp, tmp2;
      asm volatile ("   mov %[tmp], sp   \n"
                    "   mov %[tmp2], lr  \n"
                    "   msr spsr, %[psr] \n"
                    "   adr r4, 1f       \n"
#ifdef __thumb__
                    "   .inst.w 0xf3848e30    @ msr elr_hyp, r4 \n"
                    "   .inst.w 0xf3de8f00    @ eret \n"
#else
                    "   .inst 0xe12ef300 | 4   @ msr elr_hyp, r4 \n"
                    "   .inst 0xe160006e       @ eret \n"
#endif
                    "1: mov sp, %[tmp]   \n"
                    "   mov lr, %[tmp2]  \n"
                    : [tmp]"=&r"(tmp), [tmp2]"=&r"(tmp2)
                    : [psr]"r"((cpsr & ~0x1f) | 0xd3) : "cc", "r4");
    }
}

inline void create_initial_mappings()
{
  typedef Bootstrap::Phys_addr Phys_addr;
  typedef Bootstrap::Virt_addr Virt_addr;

  void *page_dir = kern_to_boot(bs_info.pi.kernel_page_directory);

  switch_to_el1();

  Virt_addr va;
  Phys_addr pa;

  // map kernel to desired virtual address
  for (va = Virt_addr(Mem_layout::Map_base),
       pa = Phys_addr(Super_pg::trunc(bs_info.kernel_start_phys));
       pa < Phys_addr(Super_pg::ceil(bs_info.kernel_end_phys));
       va += Bootstrap::map_page_size, pa += Bootstrap::map_page_size_phys)
    Bootstrap::map_memory(page_dir, va, pa, true, false);

  // Map kernel 1:1. Needed by add_initial_pmem().
  for (pa = Phys_addr(Super_pg::trunc(bs_info.kernel_start_phys));
       pa < Phys_addr(Super_pg::ceil(bs_info.kernel_end_phys));
       pa += Bootstrap::map_page_size_phys)
    Bootstrap::map_memory(page_dir, Virt_addr(cxx::int_value<Phys_addr>(pa)),
                          pa, true, true);
}
#endif // CONFIG_CPU_VIRT

#ifdef CONFIG_ARM_LPAE
#ifdef CONFIG_CPU_VIRT

inline void enable_paging(Mword pdir)
{
  disable_mmu_and_caches();

  asm volatile("mcr p15, 4, %[ttbcr], c2, c0, 2" // HTCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
  asm volatile("mcr p15, 4, %[hmair0], c10, c2, 0" // HMAIR0
               : : [hmair0] "r" (Page::Mair0_prrr_bits));
  asm volatile("mcrr p15, 4, %[pdir], %[null], c2" // HTBR
               : : [pdir]  "r" (pdir), [null]  "r" (0));
  Mem::dsb();
  asm volatile("mcr p15, 4, r0, c8, c7, 0" : : : "memory"); // TLBIALLH
  asm volatile("mcr p15, 4, r0, c8, c7, 4" : : : "memory"); // TLBIALLNSNH
  Mem::dsb();

  asm volatile("mcr p15, 4, %[control], c1, c0" // HSCTLR
      : : [control] "r" (1 | 4 | 32 | 0x1000));
  Mem::isb();
}

#else // CONFIG_CPU_VIRT

inline void enable_paging(Mword pdir)
{
  unsigned domains = 0x55555555; // client for all domains
  unsigned control = Cpu::Cp15_c1_generic;

  disable_mmu_and_caches();
  set_mair0(Page::Mair0_prrr_bits);

  asm volatile("mcr p15, 0, %[ttbcr], c2, c0, 2" // TTBCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
  Mem::dsb();
  asm volatile("mcr p15, 0, r0, c8, c7, 0"); // TLBIALL
  Mem::dsb();
  asm volatile("mcr p15, 0, %[doms], c3, c0" // domains
               : : [doms]  "r" (domains));

  asm volatile("mcrr p15, 0, %[pdir], %[null], c2" // TTBR0
               : : [pdir]  "r" (pdir), [null]  "r" (0));

  asm volatile("mcr p15, 0, %[control], c1, c0" // control
               : : [control] "r" (control));
  Mem::isb();
}

#endif // CONFIG_CPU_VIRT

inline Phys_addr init_paging(Address)
{
  void *page_dir = kern_to_boot(bs_info.pi.kernel_page_directory);
  Phys_addr *const lpae = reinterpret_cast<Phys_addr*>(kern_to_boot(bs_info.pi.kernel_lpae_dir));

  for (unsigned i = 0; i < 4; ++i)
    lpae[i] = Phys_addr((reinterpret_cast<Address>(page_dir) + 0x1000 * i) | 3);;

  create_initial_mappings();

  return Phys_addr(reinterpret_cast<Mword>(lpae));
}

#else // CONFIG_ARM_LPAE

inline void enable_paging(Mword pdir)
{
  unsigned domains = 0x55555555; // client for all domains
  unsigned control = Cpu::Cp15_c1_generic;

  disable_mmu_and_caches();

  set_asid();
  set_ttbcr();
  Mem::dsb();
  asm volatile("mcr p15, 0, r0, c8, c7, 0"); // TLBIALL
  Mem::dsb();
  asm volatile("mcr p15, 0, %[doms], c3, c0" // domains
               : : [doms]  "r" (domains));

  asm volatile("mcr p15, 0, %[pdir], c2, c0" // TTBR0
               : : [pdir] "r" (pdir));

  asm volatile("mcr p15, 0, %[control], c1, c0" // control
               : : [control] "r" (control));
  Mem::isb();
}

#if !defined (CONFIG_ARM_V7) && !defined(CONFIG_ARM_V8) && !defined(CONFIG_ARM_MPCORE)

inline Phys_addr init_paging(Address)
{
  create_initial_mappings();
  return Phys_addr(reinterpret_cast<Mword>(kern_to_boot(bs_info.pi.kernel_page_directory)));
}

#else

inline Phys_addr init_paging(Address)
{
  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(Page::Mair0_prrr_bits));
  asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(Page::Mair1_nmrr_bits));
  create_initial_mappings();

  return Phys_addr(reinterpret_cast<Mword>(kern_to_boot(bs_info.pi.kernel_page_directory)));
}

#endif // ARM_V7 ARM_V8 ARM_MPCORE
#endif // CONFIG_ARM_LPAE

#ifdef CONFIG_ARM_1176_CACHE_ALIAS_FIX
inline void do_arm_1176_cache_alias_workaround()
{
  Mword v;
  asm volatile ("mrc p15, 0, %0, c0, c0, 1   \n" : "=r" (v));
  if (v & ((1 << 23) | (1 << 11)))
    { // P bits set
      asm volatile ("mrc p15, 0, r0, c1, c0, 1   \n"
                    "orr r0, r0, #(1 << 6)       \n"
                    "mcr p15, 0, r0, c1, c0, 1   \n"
                    : : : "r0");
    }
}
#endif // CONFIG_ARM_1176_CACHE_ALIAS_FIX

struct Elf32_dyn
{
  enum { Reloc = 17 /* REL */, Reloc_count = 0x6ffffffa /* RELCOUNT */ };

  Signed32 tag;
  union {
    Unsigned32 val;
    Unsigned32 ptr;
  };
};

struct Elf32_rel
{
  Unsigned32  offset;
  Unsigned32  info;

  inline void apply(unsigned long load_addr)
  {
    auto *addr = reinterpret_cast<unsigned long *>(load_addr + offset);
    *addr += load_addr;
  }
};

static void relocate(unsigned long load_addr)
{
  Elf<Elf32_dyn, Elf32_rel>::relocate(load_addr);
}

}

asm
(
".section .text.init,\"ax\"            \n"
".p2align 2                            \n"
".type _start,#function                \n"
".global _start                        \n"
"_start:                               \n"
"     adr  r12, _start                 \n"
"     ldr  sp, .Lstack_offs            \n"
"     add  sp, sp, r12                 \n"
"     ldr  a1, .L_GOT                  \n"
"     adr  a2, .L_GOT                  \n"
"     ldr  a3, .L_GOT+4                \n"
"     add  a1, a1, a2                  \n"
"     ldr  a2, [a1, a3]                \n"
"     sub  a1, r12, a2                 \n"
"     bl   bootstrap_main              \n"

".Lstack_offs: .word (_stack - _start) \n"
".L_GOT:                               \n"
"     .word _GLOBAL_OFFSET_TABLE_ - .L_GOT \n"
"     .word _start(GOT)                \n"
".previous                             \n"
".section .bss                         \n"
".p2align 3                            \n"
"     .space 2048                      \n"
"_stack:                               \n"
".previous                             \n"
);
