#pragma once

#include <mem_space_base.h>
#include <types.h>
#include <cpu.h>

struct Mem_space_ia32_bits
{
  static Page_number canonize(Page_number v)
  { return v; }

  static void init_page_sizes()
  {
    Mem_space_base::add_page_size(Mem_space_base::Page_order(Config::PAGE_SHIFT));
    if (Cpu::cpus.cpu(Cpu_number::boot_cpu()).superpages())
      Mem_space_base::add_page_size(Mem_space_base::Page_order(22)); // 4 MiB
  }
};
