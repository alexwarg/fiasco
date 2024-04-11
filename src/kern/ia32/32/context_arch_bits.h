#pragma once

#include <types.h>
#include <vcpu.h>
#include <context_cpu_state.h>

class Context_arch_bits
{
protected:
  Context_cpu_state _cpu_state;

  explicit Context_arch_bits(Mword *kernel_sp)
  : _cpu_state(kernel_sp)
  {}

public:
  void spill_user_state()
  {}

  void fill_user_state()
  {}

  void load_segments()
  {
    _cpu_state.load_segments();
  }

  void store_segments()
  {
    _cpu_state.store_segments();
  }

  void vcpu_pv_switch_to_kernel(Vcpu_state *, bool)
  {}

  void vcpu_pv_switch_to_user(Vcpu_state *, bool)
  {}

  template<typename Context>
  void switch_cpu(Context *to)
  {
    Mword dummy1, dummy2, dummy3, dummy4;

    Context *self = static_cast<Context *>(this);
    self->update_consumed_time();

    store_segments();

    to->_cpu_state.gdt_user_entries.load();

    asm volatile
      (
       "   pushl %%ebp			\n\t"	// save base ptr of old thread
       "   pushl $1f			\n\t"	// restart addr to old stack
       "   movl  %%esp, (%0)		\n\t"	// save stack pointer
       "   movl  (%1), %%esp		\n\t"	// load new stack pointer
                                                  // in new context now (cli'd)
       "   movl  %2, %%eax		\n\t"	// new thread's "this"
       "   call  switchin_context_label	\n\t"	// switch pagetable
       "   popl  %%eax			\n\t"	// don't do ret here -- we want
       "   jmp   *%%eax			\n\t"	// to preserve the return stack
                                                  // restart code
       "  .p2align 4			\n\t"	// start code at new cache line
       "1: popl %%ebp			\n\t"	// restore base ptr

       : "=c" (dummy1), "=S" (dummy2), "=D" (dummy3), "=d" (dummy4)
       : "c" (&self->_cpu_state.kernel_sp),
         "S" (&to->_cpu_state.kernel_sp), "D" (to), "d" (self)
       : "eax", "ebx", "memory");
  }

protected:
  void arch_bits_setup_utcb_ptr(void *utcb_ptr)
  {
    _cpu_state.gdt_user_entries[_cpu_state.gdt_user_entries.Num]
      = Gdt_entry((Address)utcb_ptr, 0xfffff, Gdt_entry::Accessed,
                  Gdt_entry::Data_write, Gdt_entry::User, Gdt_entry::Code_undef,
                  Gdt_entry::Size_32, Gdt_entry::Granularity_4k);
    _cpu_state.gs = _cpu_state.fs = Gdt::gdt_utcb | 3;
  }
};
