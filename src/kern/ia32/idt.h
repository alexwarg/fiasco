#pragma once

#include <idt_init.h>
#include <mem_layout.h>
#include <types.h>
#include <x86desc.h>

#include <cassert>

class Idt_init_entry;

class Idt
{
  friend class Jdb_kern_info_bench;

public:
  static void init_table(Idt_init_entry *src, Idt_entry *idt);
  static void init();
  static void init_current_cpu();
  static void load()
  {
    Pseudo_descriptor desc(_idt, _idt_max*sizeof(Idt_entry)-1);
    set(&desc);
  }

  static void set_entry(unsigned vector, Idt_entry entry);
  static void set_entry(unsigned vector, Address addr, bool user);
  static Idt_entry const &get(unsigned vector)
  {
    assert (vector < _idt_max);
    return reinterpret_cast<Idt_entry*>(_idt)[vector];
  }

  static Address get_entry(unsigned vector)
  {
    return get(vector).offset();
  }

  static Address idt()
  {
    return _idt;
  }

  /**
   * Set IDT vector to the normal timer interrupt handler.
   */
  static void set_vectors_run();

  /**
   * IDT loading function.
   * Loads IDT base and limit into the CPU.
   * @param desc IDT descriptor (base address, limit)
   */
  static void set(Pseudo_descriptor *desc)
  {
    asm volatile ("lidt %0" : : "m" (*desc));
  }

  static void get(Pseudo_descriptor *desc)
  {
    asm volatile ("sidt %0" : "=m" (*desc) : : "memory");
  }

  static constexpr unsigned _idt_max = FIASCO_IDT_MAX;

private:
  static void set_writable(bool writable);

  constexpr static Address _idt = Mem_layout::Idt;
  static Address _idt_pa;
};

