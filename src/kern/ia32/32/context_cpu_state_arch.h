#pragma once

#include <types.h>
#include <gdt_user_entries.h>
#include <cpu.h>

template<typename BASE>
class Context_cpu_state_arch : public BASE
{
public:
  Gdt_user_entries<4> gdt_user_entries;
  Unsigned16 es, fs, gs;

  explicit Context_cpu_state_arch(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  void load_segments()
  {
    Cpu::set_es(es);
    Cpu::set_fs(fs);
    Cpu::set_gs(gs);
  }

  void store_segments()
  {
    es = Cpu::get_es();
    fs = Cpu::get_fs();
    gs = Cpu::get_gs();
  }
};

