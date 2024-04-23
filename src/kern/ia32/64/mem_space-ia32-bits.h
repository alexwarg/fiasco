#pragma once

#include <mem_space_base.h>
#include <types.h>
#include <cpu.h>

struct Mem_space_ia32_bits
{
  static Page_number canonize(Page_number v)
  {
    if (v & Page_number(Virt_addr(1UL << 48)))
      v = v | Page_number(Virt_addr(~0UL << 48));
    return v;
  }

  static void init_page_sizes()
  {
    Mem_space_base::add_page_size(Mem_space_base::Page_order(Config::PAGE_SHIFT));
    if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).superpages())
      Mem_space_base::add_page_size(Mem_space_base::Page_order(21)); // 2MB

    if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).ext_8000_0001_edx() & (1UL<<26))
      Mem_space_base::add_page_size(Mem_space_base::Page_order(30)); // 1GB
  }
};
