#pragma once

#include <gdt.h>
#include <cpu.h>
#include <x86desc.h>

template<unsigned NUM>
class Gdt_user_entries
{
public:
  static constexpr unsigned Num = NUM;
  void load()
  {
    Gdt &gdt = *Cpu::cpus.current().get_gdt();
    for (unsigned i = 0; i < NUM; ++i)
      gdt[(Gdt::gdt_user_entry1 / 8) + i] = _e[i];

    gdt[Gdt::gdt_utcb/8] = _e[NUM];
  }

  Gdt_entry const &operator [] (unsigned idx) const
  { return _e[idx]; }

  Gdt_entry &operator [] (unsigned idx)
  { return _e[idx]; }

private:
  Gdt_entry _e[NUM + 1];
};

