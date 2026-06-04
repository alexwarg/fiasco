#pragma once

#include <context_cpu_state.h>
#include <entry_frame.h>
#include <globalconfig.h>
#include <arm/32/inline_asm.h>

class Context_arch_bits
{
public:
  class Kernel_mem_op
  {
  public:
    void set_ignore(bool ignore)
    {
      _ignore = ignore;
      Mem::barrier();
    }

    bool is_ignore() const
    {
      return _ignore;
    }

    void set_hit()
    {
      _hit = true;
    }

    bool hit_and_clear()
    {
      bool h = _hit;
      if (EXPECT_FALSE(h))
        _hit = false;
      return EXPECT_FALSE(h);
    }

  private:
    bool _ignore:1;
    bool _hit:1;
  };

  Kernel_mem_op kernel_mem_op;
protected:
  Context_cpu_state _cpu_state;

  explicit Context_arch_bits(Mword *kernel_sp)
  : _cpu_state(kernel_sp)
  {}

#if defined(CONFIG_CPU_VIRT)
  void bits_fill_user_state(void *)
  {}

  void bits_spill_user_state(void *)
  {}

#else // CONFIG_CPU_VIRT
 void bits_fill_user_state(Entry_frame *ef)
  {
#ifdef __thumb__
    asm volatile ("msr SP_usr, %0 \n"
                  "msr LR_usr, %1 \n"
        : : "r"(ef->usp), "r"(ef->ulr));
#else
    asm volatile ("ldmia %[rf], {sp, lr}^"
        : : "m"(ef->usp), "m"(ef->ulr), [rf] "r" (&ef->usp));
#endif
  }

  void bits_spill_user_state(Entry_frame *ef)
  {
#ifdef __thumb__
    asm volatile ("mrs %0 ,SP_usr \n"
                  "mrs %1 ,LR_usr \n"
        : "=r"(ef->usp), "=r"(ef->ulr));
#else
    asm volatile ("stmia %[rf], {sp, lr}^"
        : "=m"(ef->usp), "=m"(ef->ulr) : [rf] "r" (&ef->usp));
#endif
  }
#endif // CONFIG_CPU_VIRT

  template<typename Context>
  void arm_switch_gp_regs(Context *to)
  {
    Context *self = static_cast<Context *>(this);
    Context_arch_bits *to_a = to;
    register void *_old_this asm("r1") = self;
    register void *_new_this asm("r0") = to;
    register void *_old_sp asm("r2") = &_cpu_state.kernel_sp;
    register void *_new_sp asm("r3") = to_a->_cpu_state.kernel_sp;

    asm volatile
      (// save context of old thread
       "   stmdb sp!, {" FIASCO_ARM_FPTR_REG "} \n" // r7 frame pointer in thumb mode
       "   adr   lr, " FIASCO_ARM_JMP_LABEL(1f) " \n" // make sure to return to thumb mode
       "   str   lr, [sp, #-4]!     \n"
       "   str   sp, [%[old_sp]]    \n"

       // switch to new stack
       "   mov   sp, %[new_sp]      \n"

       // deliver requests to new thread
       "   bl switchin_context_label \n" // call Context::switchin_context(Context *)

       // return to new context
       "   ldr   pc, [sp]           \n"
       "1:                          \n"
       "   ldr   " FIASCO_ARM_FPTR_REG ", [sp, #4]     \n"
       "   add   sp, sp, #8         \n"

       :
                "+r" (_old_this),
                "+r" (_new_this),
       [old_sp] "+r" (_old_sp),
       [new_sp] "+r" (_new_sp)
       :
       : // r11/fp is saved / restored using stmdb/ldmia
         FIASCO_ARM_CLOBBER_xFPTR, "r4", "r5", "r6", "r8", "r9",
         "r10", "r12", "r14", "memory");
  }

};

