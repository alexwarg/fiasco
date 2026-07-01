#pragma once

// ELF core dump register block for x86-64.
// NT_PRSTATUS register block = elf_gregset_t (ELF_NGREG=27, 27 x u64 = 216 bytes)
// Matches Linux struct user_regs_struct exactly, including the trailing
// fs_base/gs_base/ds/es/fs/gs fields that GDB uses to validate n_descsz.

#include <jdb_entry_frame.h>
#include <types.h>

struct Elf_Greg
{
  unsigned long long r15, r14, r13, r12, rbp, rbx;
  unsigned long long r11, r10, r9, r8;
  unsigned long long rax, rcx, rdx, rsi, rdi, orig_rax;
  unsigned long long rip, cs, eflags, rsp, ss;
  unsigned long long fs_base, gs_base, ds, es, fs, gs;
};

static constexpr unsigned short ELF_MACHINE = 62; // EM_X86_64
static constexpr unsigned char  ELF_CLASS   = 2;  // ELFCLASS64

// switch_cpu() switch frame layout at kernel_sp:
//   [ksp+ 0]  return address (RIP, label "1f")
//   [ksp+ 8]  rbp
// Real SP after popping this frame: ksp + 16.
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->rip = static_cast<unsigned long long>(ksp[0]);
  r->rbp = static_cast<unsigned long long>(ksp[1]);
  r->rsp = reinterpret_cast<unsigned long long>(ksp) + 16;
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      __builtin_memset(r, 0, sizeof(*r));
      r->r15 = ef->_r15; r->r14 = ef->_r14; r->r13 = ef->_r13;
      r->r12 = ef->_r12; r->rbp = ef->_bp;  r->rbx = ef->_bx;
      r->r11 = ef->_r11; r->r10 = ef->_r10; r->r9  = ef->_r9;
      r->r8  = ef->_r8;  r->rax = ef->_ax;  r->rcx = ef->_cx;
      r->rdx = ef->_dx;  r->rsi = ef->_si;  r->rdi = ef->_di;
      r->orig_rax = ef->_ax;
      r->rip = ef->_ip;  r->cs  = ef->_cs;  r->eflags = ef->_flags;
      r->rsp = ef->_sp;  r->ss  = ef->_ss;
      // fs_base, gs_base, ds, es, fs, gs -- not in Trap_state, leave zero
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}
