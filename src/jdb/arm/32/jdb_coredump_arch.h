#pragma once

// ELF core dump register block for ARM32.
// NT_PRSTATUS register block = struct pt_regs (18 x u32 = 72 bytes):
//   r0-r15, CPSR, orig_r0

#include <jdb_entry_frame.h>
#include <types.h>

struct Elf_Greg
{
  unsigned int uregs[18];
};

static constexpr unsigned short ELF_MACHINE = 40; // EM_ARM
static constexpr unsigned char  ELF_CLASS   = 1;  // ELFCLASS32

// arm_switch_gp_regs() switch frame layout at kernel_sp:
//   [ksp+0]  LR  -- resume address (label "1f")
//   [ksp+4]  FP  (r7 in Thumb mode, r11 in ARM mode)
// Real SP after popping this frame: ksp + 8.
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->uregs[15] = static_cast<unsigned int>(ksp[0]); // PC = LR
  r->uregs[11] = static_cast<unsigned int>(ksp[1]); // r11 = FP
  r->uregs[13] = static_cast<unsigned int>(
    reinterpret_cast<Address>(ksp) + 8);             // SP after pop
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      for (int i = 0; i <= 12; ++i)
        r->uregs[i] = static_cast<unsigned int>(ef->r[i]);
      r->uregs[13] = static_cast<unsigned int>(ef->usp);
      r->uregs[14] = static_cast<unsigned int>(ef->ulr);
      r->uregs[15] = static_cast<unsigned int>(ef->pc);
      r->uregs[16] = static_cast<unsigned int>(ef->psr);
      r->uregs[17] = static_cast<unsigned int>(ef->r[0]); // orig_r0
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}
