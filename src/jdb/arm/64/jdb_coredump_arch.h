#pragma once

// ELF core dump register block for AArch64.
// NT_PRSTATUS register block = struct user_pt_regs (34 x u64 = 272 bytes)

#include <jdb_entry_frame.h>
#include <types.h>

struct Elf_Greg
{
  unsigned long long regs[31]; // x0-x30
  unsigned long long sp, pc, pstate;
};

static constexpr unsigned short ELF_MACHINE = 183; // EM_AARCH64
static constexpr unsigned char  ELF_CLASS   = 2;   // ELFCLASS64

// arm_switch_gp_regs() switch frame layout at kernel_sp:
//   [ksp+ 0]  x30 / LR  -- resume address (label "1f")
//   [ksp+ 8]  x29 / FP
// Real SP after popping this frame: ksp + 16.
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->regs[29] = static_cast<unsigned long long>(ksp[1]); // x29 = FP
  r->regs[30] = static_cast<unsigned long long>(ksp[0]); // x30 = LR
  r->pc       = static_cast<unsigned long long>(ksp[0]); // resume PC
  r->sp       = reinterpret_cast<unsigned long long>(ksp) + 16;
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      for (int i = 0; i < 31; ++i)
        r->regs[i] = static_cast<unsigned long long>(ef->r[i]);
      r->sp     = static_cast<unsigned long long>(ef->usp);
      r->pc     = static_cast<unsigned long long>(ef->pc);
      r->pstate = static_cast<unsigned long long>(ef->pstate);
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}
