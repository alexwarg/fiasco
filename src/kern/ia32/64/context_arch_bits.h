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
  Mword *fs_base() { return &_cpu_state.fs_base; }
  Mword *gs_base() { return &_cpu_state.gs_base; }

  void spill_user_state()
  {
    _cpu_state.store_segments();
  }

  void fill_user_state()
  {
    _cpu_state.load_segments();
  }

  void load_segments()
  {}

  void store_segments()
  {}

  void vcpu_pv_switch_to_kernel(Vcpu_state *vcpu, bool current)
  {
    _cpu_state.vcpu_pv_switch_to_kernel(vcpu, current);
  }

  void vcpu_pv_switch_to_user(Vcpu_state *vcpu, bool current)
  {
    _cpu_state.vcpu_pv_switch_to_user(vcpu, current);
  }

  template<typename Context>
  void switch_cpu(Context *to)
  {
    Mword dummy1, dummy2, dummy3, dummy4;

    Context *self = static_cast<Context *>(this);
    self->update_consumed_time();
    _cpu_state.switch_segments(&to->_cpu_state);

    asm volatile
      (
       "   push %%rbp			\n\t"	// save base ptr of old thread
       "   pushq $1f			\n\t"	// push restart addr on old stack
       "   mov  %%rsp, (%[old_ksp])	\n\t"	// save stack pointer
       "   mov  (%[new_ksp]), %%rsp	\n\t"	// load new stack pointer
       // in new context now (cli'd)
       "   call  switchin_context_label	\n\t"	// switch pagetable
       "   pop   %%rax			\n\t"	// don't do ret here -- we want
       "   jmp   *%%rax			\n\t"	// to preserve the return stack
       // restart code
       "  .p2align 4			\n\t"	// start code at new cache line
       "1: pop %%rbp			\n\t"	// restore base ptr

       : "=c" (dummy1), "=a" (dummy2), "=D" (dummy3), "=S" (dummy4)
       : [old_ksp]    "c" (&self->_cpu_state.kernel_sp),
         [new_ksp]    "a" (&to->_cpu_state.kernel_sp),
         [new_thread] "D" (to),
         [old_thread] "S" (self)
       : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbx", "rdx", "memory");
  }

protected:
  void arch_bits_setup_utcb_ptr(void *utcb_ptr)
  {
    _cpu_state.gs_base = (Address)utcb_ptr;
  }
};
