#pragma once

#include <types.h>
#include <context_cpu_state.h>
#include <processor.h>
#include <asm_mips.h>
#include <context_vcpu_arch_base.h>
#include <globalconfig.h>

#include <cassert>

#ifdef CONFIG_CPU_VIRT
#include <context_mips_vz.h>
#else
#include <context_mips_novz.h>
#endif


class Context_arch_base : public Context_vcpu_arch_base
{
protected:
  Context_cpu_state _cpu_state;

  explicit Context_arch_base(Mword *kernel_sp)
  : _cpu_state(kernel_sp)
  {}
};

template<typename CTXT>
class Context_arch_x : public Context_mips_vz<CTXT, Context_arch_base>
{
private:
  using Context = CTXT;
  using Base = Context_mips_vz<CTXT, Context_arch_base>;
  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

protected:
  using Base::_cpu_state;

  Context_arch_x() noexcept
  : Base(reinterpret_cast<Mword *>(_this()->regs()))
  {}

  /**
   * Thread context switchin.  Called on every re-activation of a thread
   * (switch_exec()).  This method is public only because it is called from
   * from assembly code in switch_cpu().
   */
  void switchin_context_arch(Context *from)
  {
    from->handle_lock_holder_preemption();

    Space *spc = _this()->vcpu_aware_space();
    if (!_this()->switchin_guest_context(spc))
      // switch to our page directory if necessary
      spc->switchin_context(0);

    // load new kernel-entry SP into ErrorEPC as we use this
    // in exception entries from user mode
    Mips::mtc0((Address)(_this()->regs() + 1), Mips::Cp0_err_epc);
  }

public:
  void spill_user_state() const
  {}

  void fill_user_state() const
  {}

  void arch_update_vcpu_state(Vcpu_state *)
  {}

  void prepare_switch_to(void (*fptr)())
  {
#ifndef __mips64
    // keep the stack pointer 64-bit aligned
    --_cpu_state.kernel_sp;
#endif
    *reinterpret_cast<void(**)()> (--_cpu_state.kernel_sp) = fptr;
  }

  void switch_cpu(Context *to)
  {
    _this()->update_consumed_time();
    Proc::set_ulr(to->_cpu_state.ulr);

      {
        register void *_old_this asm("$5") = _this();     // a1
        register void *_new_this asm("$4") = to;          // a0
        register void *_old_sp asm("$6")
          = &_this()->_cpu_state.kernel_sp;  // a2
        register void *_new_sp asm("$7")
          = to->_cpu_state.kernel_sp;        // a3

        __asm__ __volatile__ (
            ".set push                                  \n"
            ".set noreorder                             \n"
            ".set noat                                  \n"
            // Stack (MIPS32: 7 words, MIPS64: 6 words):
            //   - 4 words: Parameter space for Context::switch_context(). This
            //     stack space is reserved by the caller to allow the callee to
            //     store $a0..$a3.
            //   - Only MIPS32: 1 word to keep the stack for the callee 64-bit
            //                  aligned. See also Context::prepare_switch_to().
            //   - 1 word: $28 / $gp.
            //   - 1 word: $29 / $sp.
            "  " ASM_ADDIU " $29, $29, -(%[rsz] * (%[algn]+6)) \n"
            "  " ASM_S     " $29, (%[old_sp])           \n"
            "  " ASM_S     " $28, (%[rsz] * (%[algn]+4))($29) \n"
            "  " ASM_S     " $30, (%[rsz] * (%[algn]+5))($29) \n"
            "  " ASM_LA    " $31, 1f                    \n"
            "  " ASM_S     " $31, (%[rsz] * 0)($29)     \n"
            "  " ASM_LA    " $1, switchin_context_label \n"
            "  move $29, %[new_sp]                      \n"
            "  jr $1                                    \n"
            "    " ASM_L   " $31, (%[rsz] * 0)($29)     \n" // delay slot, load ra from new stack
            "1:                                         \n"
            "  " ASM_L " $28, (%[rsz] * (%[algn]+4))($29)     \n"
            "  " ASM_L " $30, (%[rsz] * (%[algn]+5))($29)     \n"
            "  " ASM_ADDIU " $29, $29, (%[rsz] * (%[algn]+6)) \n"
            ".set pop                                   \n"
            : [old_sp]   "+r" (_old_sp),
              [new_sp]   "+r" (_new_sp),
              [old_this] "+r" (_old_this),
              [new_this] "+r" (_new_this)
            : [rsz]"i"(ASM_WORD_BYTES),
#ifdef __mips64
              [algn]"i"(0)
#else
              [algn]"i"(1)
#endif
            : "$2",  "$3",  "$8",  "$9",  "$10", "$11", "$12",
              "$13", "$14", "$15", "$16", "$17", "$18", "$19", "$20", "$21",
              "$22", "$23", "$24", "$25", "$31", "memory"
#ifndef CONFIG_CPU_MIPSR6
              , "lo", "hi"
#endif
        );
      }
  }

  void vcpu_pv_switch_to_user(Vcpu_state *, bool)
  {}

  void arch_setup_utcb_ptr()
  {
    // simulate the TLS model + TCB layout:
    // the thread pointer points + 0x7000 after the end of the TCB
    // (2x void*: dtv ptr and private ptr) before that we shall store
    // the UTCB pointer in user-land TLS and we simulate this...
    auto &u = _this()->_utcb;
    _cpu_state.ulr = (Address)&u.usr()->utcb_addr + 0x7000 + (3 * sizeof(void*));
    u.kern()->utcb_addr = (Mword)u.usr().get();
  }
};

