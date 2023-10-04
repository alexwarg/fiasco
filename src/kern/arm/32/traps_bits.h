#pragma once

#include <processor.h>
#include <globalconfig.h>
#include <fpu.h>
#include <kdb_ke.h>

inline bool
pagein_tcb_request(Return_frame *regs)
{
  //if ((*(Mword*)regs->pc & 0xfff00fff ) == 0xe5900000)
  if (*(Mword*)regs->pc == 0xe59ee000)
    {
      // printf("TCBR: %08lx\n", *(Mword*)regs->pc);
      // skip faulting instruction
      regs->pc += 4;
      // tell program that a pagefault occurred we cannot handle
      regs->psr |= 0x40000000;	// set zero flag in psr
      regs->km_lr = 0;

      return true;
    }
  return false;
}

/**
 * Mangle the error code in case of a kernel lib page fault.
 *
 * All page faults caused by code on the kernel lib page are
 * write page faults, because the code implements atomic
 * read-modify-write.
 */
inline Mword
mangle_kernel_lib_page_fault(Mword pc, Mword error_code)
{
  if (EXPECT_FALSE((pc & Kmem::Kern_lib_base) == Kmem::Kern_lib_base))
    return error_code | (1UL << 6);

  return error_code;
}


extern "C" void
slowtrap_entry(Trap_state *ts);

inline Arm_esr get_esr()
{
  Arm_esr hsr;
  asm ("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));
  return hsr;
}

#ifdef CONFIG_CPU_VIRT
#define ARM_USE_ESR_TRAPS 1
#endif

inline bool
condition_valid(unsigned char cond, Unsigned32 psr)
{
  // Matrix of instruction conditions and PSR flags,
  // index into the table is the condition from insn
  Unsigned16 v[16] =
  {
    0xf0f0,
    0x0f0f,
    0xcccc,
    0x3333,
    0xff00,
    0x00ff,
    0xaaaa,
    0x5555,
    0x0c0c,
    0xf3f3,
    0xaa55,
    0x55aa,
    0x0a05,
    0xf5fa,
    0xffff,
    0xffff
  };

  return (v[cond] >> (psr >> 28)) & 1;
}

inline bool check_and_handle_linux_cache_api(Trap_state *ts)
{
  if (Config::Support_arm_linux_cache_API)
    return false;

  if (ts->esr.ec() == 0x11 && ts->r[7] == 0xf0002)
    {
      if (ts->r[2] == 0)
        Mem_op::arm_mem_cache_maint(Mem_op::Op_cache_coherent,
                                    (void *)ts->r[0], (void *)ts->r[1]);
      ts->r[0] = 0;
      return true;
    }

  return false;
}



#ifdef CONFIG_CPU_VIRT
#ifdef CONFIG_ARM_V8PLUS

inline bool invalid_pfa(Arm_esr hsr)
{
  // FSC == 0b010001 is only documented for data aborts so this code works also
  // for instruction aborts.
  return hsr.pf_fsc() == 0x11 || hsr.pf_fnv();
}

#else // CONFIG_ARM_V8PLUS

inline bool invalid_pfa(Arm_esr)
{
  return false;
}

#endif // CONFIG_ARM_V8PLUS

inline bool
is_syscall_pc(Address pc)
{
  return Address(-0x0c) <= pc && pc <= Address(-0x08);
}

inline Address
get_fault_pfa(Arm_esr hsr, bool insn_abt, bool ext_vcpu)
{
  if (invalid_pfa(hsr))
    return ~0UL;

  Unsigned32 far;
  if (insn_abt)
    asm ("mrc p15, 4, %0, c6, c0, 2" : "=r" (far));
  else
    asm ("mrc p15, 4, %0, c6, c0, 0" : "=r" (far));

  if (EXPECT_TRUE(!ext_vcpu))
    return far;

  Unsigned32 sctlr;
  asm ("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctlr));
  if (!(sctlr & 1)) // stage 1 mmu disabled
    return far;

  if (hsr.pf_s1ptw()) // stage 1 walk
    {
      Unsigned32 ipa;
      asm ("mrc p15, 4, %0, c6, c0, 4" : "=r" (ipa));
      return ipa << 8;
    }

  if ((hsr.pf_fsc() & 0x3c) != 0xc) // no permission fault
    {
      Unsigned32 ipa;
      asm ("mrc p15, 4, %0, c6, c0, 4" : "=r" (ipa));
      return (ipa << 8) | (far & 0xfff);
    }

  Unsigned64 par, tmp;
  asm ("mrrc p15, 0, %Q0, %R0, c7 \n" // save guest PAR
       "mcr p15, 0, %2, c7, c8, 0 \n" // write guest virtual address to ATS1CPR
       "isb                       \n"
       "mrrc p15, 0, %Q1, %R1, c7 \n" // read translation result from PAR
       "mcrr p15, 0, %Q0, %R0, c7 \n" // restore guest PAR
       : "=&r"(tmp), "=r"(par)
       : "r"(far));
  if (par & 1)
    return ~0UL;
  return (par & 0xfffff000UL) | (far & 0xfff);
}

inline Mword
get_lr_for_mode(Return_frame const *rf)
{
  Mword ret;
  switch (rf->psr & 0x1f)
    {
    case Proc::PSR_m_usr:
    case Proc::PSR_m_sys:
      return rf->ulr;
    case Proc::PSR_m_irq:
      asm ("mrs %0, lr_irq" : "=r" (ret)); return ret;
    case Proc::PSR_m_fiq:
      asm ("mrs %0, lr_fiq" : "=r" (ret)); return ret;
    case Proc::PSR_m_abt:
      asm ("mrs %0, lr_abt" : "=r" (ret)); return ret;
    case Proc::PSR_m_svc:
      asm ("mrs %0, lr_svc" : "=r" (ret)); return ret;
    case Proc::PSR_m_und:
      asm ("mrs %0, lr_und" : "=r" (ret)); return ret;
    default:
      assert(false); // wrong processor mode
      return ~0UL;
    }
}

inline void handle_svc(Context *c, Trap_state *ts)
{
  Unsigned32 pc = ts->pc;
  if (!is_syscall_pc(pc))
    {
      slowtrap_entry(ts);
      return;
    }
  ts->pc = get_lr_for_mode(ts);
  Mword state = c->state.dirty();
  c->state.del(Thread_cancel);
  if (state & Thread_vcpu_user)
    {
      slowtrap_entry(ts);
      return;
    }

  typedef void Syscall(void);
  extern Syscall *sys_call_table[];
  sys_call_table[(-pc) / 4]();
}

inline bool
check_and_handle_undef_syscall(Return_frame *rf)
{
  Mword pc = rf->pc;
  if (!is_syscall_pc(pc + 4))
    return false;

  rf->pc = get_lr_for_mode(rf);
  typedef void Syscall(void);
  extern Syscall *sys_call_table[];
  sys_call_table[-(pc + 4) / 4]();
  return true;
}

static bool
handle_fpu_trap(Trap_state *ts)
{
  unsigned cond = ts->esr.cv() ? ts->esr.cond() : 0xe;
  if (!condition_valid(cond, ts->psr))
    {
      // FPU insns are 32bit, even for thumb
      assert (ts->esr.il());
      ts->pc += 4;
      return true;
    }

  assert (!Fpu::is_enabled());

  if (current_thread()->switchin_fpu())
    return true;

  // emulate the ARM exception entry PC
  ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;

  return false;
}

extern "C" void hyp_mode_fault(Mword abort_type, Trap_state *ts);
void hyp_mode_fault(Mword abort_type, Trap_state *ts)
{
  Mword v;

  Mword hsr;
  asm volatile("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));

  switch (abort_type)
    {
    case 0:
    case 1:
      ts->esr.ec() = abort_type ? 0x11 : 0;
      printf("KERNEL%d: %s fault at lr=%lx pc=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             abort_type ? "SWI" : "Undefined instruction",
             ts->km_lr, ts->pc, hsr);
      break;
    case 2:
      ts->esr.ec() = 0x21;
      asm volatile("mrc p15, 4, %0, c6, c0, 2" : "=r"(v));
      printf("KERNEL%d: Instruction abort at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             v, hsr);
      break;
    case 3:
      ts->esr.ec() = 0x25;
      asm volatile("mrc p15, 4, %0, c6, c0, 0" : "=r"(v));
      printf("KERNEL%d: Data abort: pc=%lx pfa=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             ts->ip(), v, hsr);
      break;
    default:
      printf("KERNEL%d: Unknown hyp fault at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             ts->ip(), hsr);
      break;
    };

  ts->dump();

  kdb_ke("In-kernel fault");
}
#ifdef CONFIG_FPU

template<typename T>
inline T peek_user(T const *adr, Context *c)
{
  Address pa;
  asm ("mcr p15, 0, %1, c7, c8, 6 \n"
       "mrc p15, 0, %0, c7, c4, 0 \n"
       : "=r" (pa) : "r"(adr) );
  if (EXPECT_TRUE(!(pa & 1)))
    return *reinterpret_cast<T const *>(cxx::mask_lsb(pa, 12)
                                        | cxx::get_lsb((Address)adr, 12));

  c->kernel_mem_op.set_hit();
  return T(~0);
}
#endif // CONFIG_FPU
#else // CONFIG_CPU_VIRT
#ifdef CONFIG_FPU

template<typename T>
inline T peek_user(T const *adr, Context *c)
{
  T v;
  c->kernel_mem_op.set_ignore(true);
  v = *adr;
  c->kernel_mem_op.set_ignore(false);
  return v;
}
#endif

#endif // CONFIG_CPU_VIRT

#ifdef CONFIG_FPU
static bool
handle_fpu_trap(Unsigned32 opcode, Trap_state *ts)
{
  if (!condition_valid(opcode >> 28, ts->psr))
    {
      // FPU insns are 32bit, even for thumb
      if (ts->psr & Proc::Status_thumb)
        ts->pc += 2;
      return true;
    }

  if (Fpu::is_enabled())
    {
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);

      ts->esr.ec() = 0; // tag fpu undef insn
    }
  else if (current_thread()->switchin_fpu())
    {
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);
      ts->pc -= (ts->psr & Proc::Status_thumb) ? 2 : 4;
      return true;
    }
  else
    {
      ts->esr.ec() = 0x07;
      ts->esr.cond() = opcode >> 28;
      ts->esr.cv() = 1;
      ts->esr.cpt_cpnr() = 10;
    }

  return false;
}

inline bool
check_for_kernel_mem_access_pf(Trap_state *ts, Thread *t)
{
  if (!t->kernel_mem_op.hit_and_clear())
    return false;

  assert (cxx::as_type<Return_frame *>(ts) == t->regs());
  Mword pc = t->user_ip();
  pc -= (ts->psr & Proc::Status_thumb) ? 2 : 4;
  t->user_ip(pc);
  return true;
}

inline bool
check_and_handle_coproc_faults(Thread *c, Trap_state *ts)
{
  if (!ts->exception_is_undef_insn())
    return false;

  Unsigned32 opcode;

  if (ts->psr & Proc::Status_thumb)
    {
      Unsigned16 v = peek_user((Unsigned16 *)(ts->pc - 2), c);

      if (EXPECT_FALSE(check_for_kernel_mem_access_pf(ts, c)))
        return true;

      if ((v >> 11) <= 0x1c)
        return false;

      opcode = (v << 16) | peek_user((Unsigned16 *)ts->pc, c);
    }
  else
    opcode = peek_user((Unsigned32 *)(ts->pc - 4), c);

  if (EXPECT_FALSE(check_for_kernel_mem_access_pf(ts, c)))
    return true;

  if (ts->psr & Proc::Status_thumb)
    {
      if (   (opcode & 0xef000000) == 0xef000000 // A6.3.18
          || (opcode & 0xff100000) == 0xf9000000)
        return handle_fpu_trap(opcode, ts);
    }
  else
    {
      if (   (opcode & 0xfe000000) == 0xf2000000 // A5.7.1
          || (opcode & 0xff100000) == 0xf4000000)
        return handle_fpu_trap(opcode, ts);
    }

  if ((opcode & 0x0c000e00) == 0x0c000a00)
    return handle_fpu_trap(opcode, ts);

  return false;
}

#else // CONFIG_FPU

inline bool
check_and_handle_coproc_faults(Thread *, Trap_state *)
{
  return false;
}

#endif // CONFIG_FPU

