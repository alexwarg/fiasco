#pragma once

// ELF core dump register block for i386.
// NT_PRSTATUS register block = struct user_regs_struct (17 x u32 = 68 bytes)

#include <jdb_entry_frame.h>
#include <types.h>

struct Elf_Greg
{
  unsigned int ebx, ecx, edx, esi, edi, ebp, eax;
  unsigned int xds, xes, xfs, xgs, orig_eax;
  unsigned int eip, xcs, eflags, esp, xss;
};

static constexpr unsigned short ELF_MACHINE = 3; // EM_386
static constexpr unsigned char  ELF_CLASS   = 1; // ELFCLASS32

// switch_cpu() switch frame layout at kernel_sp:
//   [ksp+0]  return address (EIP, label "1f")
//   [ksp+4]  ebp
// Real SP after popping this frame: ksp + 8.
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->eip = static_cast<unsigned int>(ksp[0]);
  r->ebp = static_cast<unsigned int>(ksp[1]);
  r->esp = static_cast<unsigned int>(
    reinterpret_cast<Address>(ksp) + 8);
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      r->ebx = ef->_bx;  r->ecx = ef->_cx;  r->edx = ef->_dx;
      r->esi = ef->_si;  r->edi = ef->_di;  r->ebp = ef->_bp;
      r->eax = ef->_ax;
      r->xds = r->xes = r->xfs = r->xgs = 0;
      r->orig_eax = ef->_ax;
      r->eip = ef->_ip;  r->xcs = ef->_cs;  r->eflags = ef->_flags;
      r->esp = ef->_sp;  r->xss = ef->_ss;
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}
