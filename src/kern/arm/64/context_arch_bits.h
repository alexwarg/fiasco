#pragma once

#include <context_cpu_state.h>
#include <entry_frame.h>

class Context_arch_bits
{
protected:
  Context_cpu_state _cpu_state;

  explicit Context_arch_bits(Mword *kernel_sp)
  : _cpu_state(kernel_sp)
  {}

  void bits_fill_user_state(Entry_frame const *ef)
  {
    asm volatile ("msr SP_EL0, %[rf]"
                  : : [rf] "r" (ef->usp));
  }

  void bits_spill_user_state(Entry_frame *ef)
  {
    asm volatile ("mrs %[rf], SP_EL0"
                  : [rf] "=r" (ef->usp));
  }


  template<typename Context>
  void arm_switch_gp_regs(Context *to)
  {
    Context *self = static_cast<Context *>(this);
    Context_arch_bits *to_a = to;
    register Mword _old_this asm("x1") = (Mword)self;
    register Mword _new_this asm("x0") = (Mword)to;
    register unsigned long dummy1 asm ("x9");
    register unsigned long dummy2 asm ("x10");

    asm volatile
      (// save context of old thread
       "   adr   x30, 1f            \n"
       "   str   x30, [sp, #-16]!   \n"
       "   str   x29, [sp, #8]      \n" // FP
       "   mov   x29, sp            \n"
       "   str   x29, [%[old_sp]]   \n"

       // switch to new stack
       "   mov   sp, %[new_sp]      \n"

       // deliver requests to new thread
       "   bl switchin_context_label \n" // call Context::switchin_context(Context *)

       // return to new context
       "   ldr   x29, [sp, #8]      \n"
       "   ldr   x30, [sp], #16     \n"
       "   br    x30                \n"
       "1: \n"

       :
                    "=r" (_old_this),
                    "=r" (_new_this),
       [old_sp]     "=r" (dummy1),
       [new_sp]     "=r" (dummy2)
       :
       "0" (_old_this),
       "1" (_new_this),
       "2" (&_cpu_state.kernel_sp),
       "3" (to_a->_cpu_state.kernel_sp)
       :  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",
         "x11", "x12", "x13", "x14", "x15", "x16", "x17",
         "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25",
         "x26", "x27", "x28", "x30", "memory");
  }

};
