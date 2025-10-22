#include <fpu_arch.h>
#include <fpu.h>
#include <globalconfig.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "mem.h"
#include "processor.h"
#include <trap_state.h>

#ifdef CONFIG_LAZY_FPU

inline void finish_init(Fpu &f)
{
  f.disable();
  f.set_owner(0);
}

#else

inline void finish_init(Fpu &)
{}

#endif

#ifdef CONFIG_ARM_V6PLUS
#ifdef CONFIG_CPU_VIRT

inline void copro_enable()
{
  Mword r;
  asm volatile("mrc  p15, 0, %0, c1, c0, 2\n"
               "orr  %0, %0, %1           \n"
               "mcr  p15, 0, %0, c1, c0, 2\n"
               : "=r" (r) : "I" (0x00f00000));
  Mem::isb();
  Fpu_arch::fpexc((Fpu_arch::fpexc() | Fpu_arch::FPEXC_EN)); // & ~FPEXC_EX);
}

#else // CONFIG_CPU_VIRT

inline void copro_enable()
{
  Mword r;
  asm volatile("mrc  p15, 0, %0, c1, c0, 2\n"
               "orr  %0, %0, %1           \n"
               "mcr  p15, 0, %0, c1, c0, 2\n"
               : "=r" (r) : "I" (0x00f00000));
  Mem::isb();
}

#endif // CONFIG_CPU_VIRT
#else // CONFIG_ARM_V6PLUS

inline void copro_enable()
{}

#endif // CONFIG_ARM_V6PLUS

bool Fpu_arch::save_32r;

inline Mword fpsid_read()
{
  Mword v;
  asm volatile(".fpu vfp\n vmrs %0, fpsid" : "=r" (v));
  return v;
}

void
Fpu_arch::init(Cpu_number cpu, bool resume)
{
  if (Config::Jdb && !resume && cpu == Cpu_number::boot_cpu())
    printf("FPU: Initialize\n");

  copro_enable();

  Fpu &f = Fpu::fpu.cpu(cpu);
  f._fpsid = Fpsid(fpsid_read());
  if (cpu == Cpu_number::boot_cpu() && f._fpsid.arch_version() > 1)
    save_32r = (mvfr0() & 0xf) == 2;

  finish_init(f);
}

inline void
save_fpu_regs(Fpu_arch::Fpu_regs *r, bool save_32r)
{
  Mword tmp;
  asm volatile(".fpu neon\n"
               "cmp    %2, #0          \n"
               "vstm   %0!, {d0-d15}   \n"
               "beq 1f                 \n"
               "vstm   %0!, {d16-d31}  \n"
               "1:                     \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r));
}

inline void
restore_fpu_regs(Fpu_arch::Fpu_regs const *r, bool save_32r)
{
  Mword tmp;
  asm volatile(".fpu neon\n "
               "cmp    %2, #0        \n"
               "vldm   %0!, {d0-d15}  \n"
               "beq 1f                 \n"
               "vldm   %0!, {d16-d31} \n"
               "1:                     \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r));
}

void
Fpu_arch::save_state(Fpu_state *fpu_regs)
{
  Mword tmp;

  assert(fpu_regs);

  asm volatile (".fpu vfp \n"
                "vmrs %[fpexc], fpexc  \n"
                "orr %[tmp], %[fpexc], #0x40000000   \n"
                "vmsr fpexc, %[tmp]    \n" // enable FPU to store full state
                "vmrs %[fpscr], fpscr  \n"
                : [tmp] "=&r" (tmp),
                  [fpexc] "=&r" (fpu_regs->fpexc),
                  [fpscr] "=&r" (fpu_regs->fpscr));

  if (fpu_regs->fpexc & FPEXC_EX)
    {
      fpu_regs->fpinst = fpinst();
      if (fpu_regs->fpexc & FPEXC_FP2V)
        fpu_regs->fpinst2 = fpinst2();
    }

  save_fpu_regs(fpu_regs, save_32r);
}

Fpu_arch::Emulate_result
Fpu_arch::emulate_insns(Mword opcode, Trap_state *ts)
{
  unsigned rt = (opcode >> 12) & 0xf;
  Fpsid fpsid = Fpu::fpu.current().fpsid();
  switch (opcode & 0x0fff'0f90)
    {
    case 0x0ef0'0a10: // FPSID
      ts->r[rt] = fpsid.v;
      break;
    case 0x0ef6'0a10: // MVFR1
      if (fpsid.arch_version() < 2)
        return Emulate_result::Undefined;
      ts->r[rt] = Fpu::mvfr1();
      break;
    case 0x0ef7'0a10: // MVFR0
      if (fpsid.arch_version() < 2)
        return Emulate_result::Undefined;
      ts->r[rt] = Fpu::mvfr0();
      break;
    default:
      return Emulate_result::Unknown;
    }

  if (ts->psr & Proc::Status_thumb)
    ts->pc += 2;

  return Emulate_result::Emulated;
}

void
Fpu_arch::save_user_exception_state(bool owner, Fpu_state_ptr const &s, Trap_state *ts, Exception_state_user *esu)
{
  if (!(ts->esr.ec() == 7 && ts->esr.cpt_cpnr() == 10))
    return;

  if (owner)
    {
      if (Proc::Is_hyp && !is_enabled())
        Fpu::fpu.current().enable();

      Mword exc = Fpu::fpexc();

      esu->fpexc = exc;
      if (exc & FPEXC_EX)
        {
          esu->fpinst  = Fpu::fpinst();
          if (exc & FPEXC_FP2V)
            esu->fpinst2 = Fpu::fpinst2();

          if (!Proc::Is_hyp)
            Fpu::fpexc(exc & ~FPEXC_EX);
        }
      return;
    }

  if (!s)
    {
      esu->fpexc = 0;
      return;
    }

  assert (s);

  Fpu_regs *fpu_regs = s.get();
  esu->fpexc = fpu_regs->fpexc;
  if (fpu_regs->fpexc & FPEXC_EX)
    {
      esu->fpinst  = fpu_regs->fpinst;
      if (fpu_regs->fpexc & FPEXC_FP2V)
        esu->fpinst2 = fpu_regs->fpinst2;

      if (!Proc::Is_hyp)
        fpu_regs->fpexc &= ~FPEXC_EX;
    }
}

#ifdef CONFIG_CPU_VIRT

void
Fpu_arch::restore_state(Fpu_state const *fpu_regs, bool)
{
  assert(fpu_regs);

  asm volatile (".fpu vfp\n"
                "vmsr fpexc, %[fpexc]  \n"
                "vmsr fpscr, %[fpscr]  \n"
                :
                : [fpexc] "r" (fpu_regs->fpexc | FPEXC_EN),
                  [fpscr] "r" (fpu_regs->fpscr));

  if (fpu_regs->fpexc & FPEXC_EX)
    {
      fpinst(fpu_regs->fpinst);
      if (fpu_regs->fpexc & FPEXC_FP2V)
        fpinst2(fpu_regs->fpinst2);
    }

  restore_fpu_regs(fpu_regs, save_32r);

  asm volatile (".fpu vfp\n  vmsr fpexc, %0  \n"
                :
                : "r" (fpu_regs->fpexc));
}

#else // CONFIG_CPU_VIRT

void
Fpu_arch::restore_state(Fpu_state const *fpu_regs, bool)
{
  assert(fpu_regs);

  restore_fpu_regs(fpu_regs, save_32r);

  asm volatile (".fpu vfp        \n"
                "vmsr fpexc, %0  \n"
                "vmsr fpscr, %1  \n"
                :
                : "r" ((fpu_regs->fpexc | FPEXC_EN) & ~FPEXC_EX),
                  "r" (fpu_regs->fpscr));
}

#endif // CONFIG_CPU_VIRT

