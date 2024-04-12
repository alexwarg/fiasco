#include <context.h>
#include <thread.h>
#include <mem_op.h>
#include <thread_vcpu.h>
#include <processor.h>
#include <traps_bits.h>

#include <globalconfig.h>
#include <nested_trap_handler.h>

#ifdef CONFIG_ARM_LPAE

inline Mword
map_fsr_user(Mword fsr, bool is_only_pf)
{
  static Unsigned16 const pf_map[32] =
  {
    /*  0x0 */ 0,
    /*  0x1 */ 0x21, /* Alignment */
    /*  0x2 */ 0x22, /* Debug */
    /*  0x3 */ 0x08, /* Access flag (1st level) */
    /*  0x4 */ 0x2000, /* Insn cache maint */
    /*  0x5 */ 0x04, /* Transl (1st level) */
    /*  0x6 */ 0x09, /* Access flag (2nd level) */
    /*  0x7 */ 0x05, /* Transl (2nd level) */
    /*  0x8 */ 0x10, /* Sync ext abort */
    /*  0x9 */ 0x3c, /* Domain (1st level) */
    /*  0xa */ 0,
    /*  0xb */ 0x3d, /* Domain (2nd level) */
    /*  0xc */ 0x14, /* Sync ext abt on PT walk (1st level) */
    /*  0xd */ 0x0c, /* Perm (1st level) */
    /*  0xe */ 0x15, /* Sync ext abt on PT walk (2nd level) */
    /*  0xf */ 0x0d, /* Perm (2nd level) */
    /* 0x10 */ 0x30, /* TLB conflict abort */
    /* 0x11 */ 0,
    /* 0x12 */ 0,
    /* 0x13 */ 0,
    /* 0x14 */ 0x34, /* Lockdown (impl-def) */
    /* 0x15 */ 0,
    /* 0x16 */ 0x11, /* Async ext abort */
    /* 0x17 */ 0,
    /* 0x18 */ 0x19, /* Async par err on mem access */
    /* 0x19 */ 0x18, /* Sync par err on mem access */
    /* 0x1a */ 0x3a, /* Copro abort (impl-def) */
    /* 0x1b */ 0,
    /* 0x1c */ 0x14, /* Sync par err on PT walk (1st level) */
    /* 0x1d */ 0,
    /* 0x1e */ 0x15, /* Sync par err on PT walk (2nd level) */
    /* 0x1f */ 0,
  };

  if (is_only_pf || (fsr & 0xc0000000) == 0x80000000)
    return pf_map[((fsr >> 10) & 1) | (fsr & 0xf)] | (fsr & ~0x43f);

  return fsr;
}

#else // CONFIG_ARM_LPAE

inline Mword map_fsr_user(Mword fsr, bool)
{ return fsr; }

#endif // CONFIG_ARM_LPAE

inline
Mword user_pagefault_entry(Mword pfa, Mword error_code, Mword pc)
{
  if (!IS_ENABLED(CONFIG_ARM_LPAE)
      && EXPECT_FALSE(Thread::is_debug_exception(error_code, true)))
    return 0;

  // Pagefault in user mode
  error_code = Thread::mangle_kernel_lib_page_fault(pc, error_code);

  Thread *t = current_thread();
  // TODO: Avoid calling Thread::map_fsr_user here everytime!
  if (Thread_vcpu::vcpu_pagefault(t, pfa, map_fsr_user(error_code, true), pc))
    return 1;

  if (Mem_layout::in_kernel(pfa))
      return 0;

  t->state.del(Thread_cancel);
  Proc::sti();

  return t->handle_user_space_page_fault(pfa, error_code);
}

void slowtrap_entry(Trap_state *ts)
{
  ts->error_code = map_fsr_user(ts->error_code, false);

  if (0)
    printf("Trap: pfa=%08lx pc=%08lx err=%08lx psr=%lx\n", ts->pf_address, ts->pc, ts->error_code, ts->psr);
  Thread *t = current_thread();

  LOG_TRAP;

  if (Config::Support_arm_linux_cache_API)
    {
      if (   ts->esr.ec() == 0x11
          && ts->r[7] == 0xf0002)
        {
          if (ts->r[2] == 0)
            Mem_op::arm_mem_cache_maint(Mem_op::Op_cache_coherent,
                                        (void *)ts->r[0], (void *)ts->r[1]);
          ts->r[0] = 0;
          return;
        }
    }

  if (check_and_handle_coproc_faults(t, ts))
    return;

  if (Thread::is_debug_exception(ts->error_code))
    {
      call_nested_trap_handler(ts);
      return;
    }

  // send exception IPC if requested
  if (t->send_exception(ts))
    return;

  t->kill();
}

#ifdef ARM_USE_ESR_TRAPS

extern "C" void arm_esr_entry(Return_frame *rf);
void arm_esr_entry(Return_frame *rf)
{
  Trap_state *ts = static_cast<Trap_state*>(rf);
  Thread *ct = current_thread();

  Arm_esr esr = get_esr();
  ts->error_code = esr.raw();

  Mword tmp;
  Mword state = ct->state();

  switch (esr.ec())
    {
    case 0x20: // Instruction abort from a lower exception level
      tmp = get_fault_pfa(esr, true, state & Thread_ext_vcpu_enabled);
      if (!user_pagefault_entry(tmp, esr.raw(), rf->pc))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x24: // Data abort from a lower exception Level
      tmp = get_fault_pfa(esr, false, state & Thread_ext_vcpu_enabled);
      if (!user_pagefault_entry(tmp, esr.raw(), rf->pc))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x11: // SVC instruction execution on AArch32
    case 0x12: // HVC instruction execution on AArch32
    case 0x15: // SVC instruction execution on AArch64
    case 0x16: // HVC instruction execution on AArch64
      handle_svc(current(), ts);
      return;

    case 0x00: // Unknown reason, undefined opcode with HCR.TGE=1
        {
          ct->state.del(Thread_cancel);
          Mword state = ct->state();
          Unsigned32 pc = rf->pc;

          if (state & (Thread_vcpu_user | Thread_alien))
            {
              ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;
              ct->send_exception(ts);
              return;
            }
          else if (EXPECT_FALSE(!is_syscall_pc(pc + 4)))
            {
              ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;
              slowtrap_entry(ts);
              return;
            }

          rf->pc = get_lr_for_mode(rf);
          ct->state.del(Thread_cancel);
          typedef void Syscall(void);
          extern Syscall *sys_call_table[];
          sys_call_table[-(pc + 4) / 4]();
          return;
        }
      break;

    case 0x07: // SVE, Advanced SIMD or floating-point trap
      if ((Proc::Is_64bit // Always FPU trap on Aarch64, not used for other CPs.
           || esr.cpt_simd()
           || esr.cpt_cpnr() == 10  // CP10: Floating-point
           || esr.cpt_cpnr() == 11) // CP11: Advanced SIMD
          && handle_fpu_trap(ts))
        return;

      ct->send_exception(ts);
      break;

    default:
      ct->send_exception(ts);
      break;
    }
}

#else // ARM_USE_ESR_TRAPS

inline
Mword kern_pagefault_entry(Mword pfa, Mword error_code,
                           Mword pc, Return_frame *ret_frame)
{
  if (EXPECT_FALSE(PF::is_alignment_error(error_code)))
    {
      WARNX(Warning,
            "KERNEL%d: alignment error at %08lx (PC: %08lx, SP: %08lx, FSR: %lx, PSR: %lx)\n",
            cxx::int_value<Cpu_number>(current_cpu()), pfa, pc,
            ret_frame->usp, error_code, ret_frame->psr);
      return 0;
    }

  if (!IS_ENABLED(CONFIG_ARM_LPAE)
      && EXPECT_FALSE(Thread::is_debug_exception(error_code, true)))
    return 0;

  Thread *t = current_thread();

  // Check for page fault in user memory area
  if (EXPECT_TRUE(!Mem_layout::in_kernel(pfa)))
    {
      Proc::sti();
      return t->handle_user_space_page_fault(pfa, error_code);
    }

  if (Mem_layout::is_caps_area(pfa))
    {
      // Test for special case -- see function documentation
      if (t->pagein_tcb_request(ret_frame))
        return 2;

      printf("Fiasco BUG: Invalid CAP access (pc=%lx, pfa=%lx)\n", pc, pfa);
      kdb_ke("Fiasco BUG: Invalid access to Caps area");
      return 0;
    }

  // cache operations we carry out for user space might cause PFs, we just
  // ignore those
  if (EXPECT_FALSE(t->kernel_mem_op.is_ignore()))
    {
      t->kernel_mem_op.set_hit();
      ret_frame->pc += 4;
      return 1;
    }

  t->do_recover_jmp_buf();
  return 0;
}

/**
 * The low-level page fault handler called from entry.S.  We're invoked with
 * interrupts turned off.  Apart from turning on interrupts in almost
 * all cases (except for kernel page faults in TCB area), just forwards
 * the call to Thread::handle_page_fault().
 * @param pfa page-fault virtual address
 * @param error_code CPU error code
 * @return true if page fault could be resolved, false otherwise
 */
extern "C" Mword
pagefault_entry(Mword pfa, Mword error_code,
                Mword pc, Return_frame *ret_frame);

[[gnu::flatten]]
Mword pagefault_entry(Mword pfa, Mword error_code,
                      Mword pc, Return_frame *ret_frame)
{
  if (EXPECT_TRUE(PF::is_usermode_error(error_code)))
    return user_pagefault_entry(pfa, error_code, pc);
  else
    return kern_pagefault_entry(pfa, error_code, pc, ret_frame);
}

#endif // ARM_USE_ESR_TRAPS
