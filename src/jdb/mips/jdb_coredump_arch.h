#pragma once

// Architecture-specific ELF core dump definitions for MIPS32 and MIPS64.
// Included by jdb/jdb_coredump.cc via the search-path include <jdb_coredump_arch.h>.
//
// Register indices are taken from Linux uapi/asm/reg.h:
//
// MIPS64 (n64 ABI): ELF_NGREG=45, u64[45] = 360 bytes; slots 38-44 unused
//   [0..31]  $r0..$r31
//   [32]     lo
//   [33]     hi
//   [34]     cp0_epc   <-- PC
//   [35]     cp0_badvaddr
//   [36]     cp0_status
//   [37]     cp0_cause
//
// MIPS32 (o32 ABI): flat u32[45], EF_SIZE = 180 bytes
//   [0..5]   unused (struct pt_regs header fields: stackargs etc.)
//   [6..37]  $r0..$r31
//   [38]     lo
//   [39]     hi
//   [40]     cp0_epc   <-- PC
//   [41]     cp0_badvaddr
//   [42]     cp0_status
//   [43]     cp0_cause
//   [44]     unused0

#include <jdb_entry_frame.h>
#include <globalconfig.h>
#include <types.h>

#if defined(CONFIG_BIT64)

// MIPS64: ELF_NGREG=45, 45 x u64 = 360 bytes; slots 38-44 are unused
struct Elf_Greg { unsigned long long r[45]; };

static constexpr unsigned short ELF_MACHINE = 8; // EM_MIPS
static constexpr unsigned char  ELF_CLASS   = 2; // ELFCLASS64

// switch_cpu() MIPS64 switch frame layout (6 words x 8 bytes = 48 bytes):
//   [ksp+ 0]  $ra ($31) -- resume PC (label "1f")
//   [ksp+ 8..24] parameter space ($a0..$a3 spill slots, 3 words)
//   [ksp+32]  $gp ($28)
//   [ksp+40]  $s8/$fp ($30)
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->r[31] = static_cast<unsigned long long>(ksp[0]); // $ra
  r->r[28] = static_cast<unsigned long long>(ksp[4]); // $gp
  r->r[30] = static_cast<unsigned long long>(ksp[5]); // $s8/$fp
  r->r[29] = reinterpret_cast<unsigned long long>(ksp) + 6 * 8; // $sp after pop
  r->r[34] = static_cast<unsigned long long>(ksp[0]); // cp0_epc = resume PC
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      __builtin_memset(r, 0, sizeof(*r));
      for (int i = 0; i < 32; ++i)
        r->r[i] = static_cast<unsigned long long>(ef->r[i]);
      r->r[32] = static_cast<unsigned long long>(ef->lo);
      r->r[33] = static_cast<unsigned long long>(ef->hi);
      r->r[34] = static_cast<unsigned long long>(ef->epc);       // PC
      r->r[35] = static_cast<unsigned long long>(ef->bad_v_addr);
      r->r[36] = static_cast<unsigned long long>(ef->status);
      r->r[37] = static_cast<unsigned long long>(ef->cause);
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}

#else

// MIPS32: 45 x u32 = 180 bytes
struct Elf_Greg { unsigned int r[45]; };

static constexpr unsigned short ELF_MACHINE = 8; // EM_MIPS
static constexpr unsigned char  ELF_CLASS   = 1; // ELFCLASS32

// switch_cpu() MIPS32 switch frame layout (7 words x 4 bytes = 28 bytes):
//   [ksp+ 0]  $ra ($31) -- resume PC (label "1f")
//   [ksp+ 4..16] parameter space ($a0..$a3 spill slots, 4 words)
//   [ksp+20]  $gp ($28)
//   [ksp+24]  $s8/$fp ($30)
static void fill_greg_from_switch_frame(Elf_Greg *r, Mword *ksp)
{
  __builtin_memset(r, 0, sizeof(*r));
  r->r[6  + 31] = static_cast<unsigned int>(ksp[0]); // $ra
  r->r[6  + 28] = static_cast<unsigned int>(ksp[5]); // $gp
  r->r[6  + 30] = static_cast<unsigned int>(ksp[6]); // $s8/$fp
  r->r[6  + 29] = static_cast<unsigned int>(          // $sp after pop
    reinterpret_cast<Address>(ksp) + 7 * 4);
  r->r[40]      = static_cast<unsigned int>(ksp[0]);  // cp0_epc = resume PC
}

static void fill_greg(Elf_Greg *r, Jdb_entry_frame *ef, Mword *switch_ksp)
{
  if (ef)
    {
      __builtin_memset(r, 0, sizeof(*r));
      for (int i = 0; i < 32; ++i)
        r->r[6 + i] = static_cast<unsigned int>(ef->r[i]);
      r->r[38] = static_cast<unsigned int>(ef->lo);
      r->r[39] = static_cast<unsigned int>(ef->hi);
      r->r[40] = static_cast<unsigned int>(ef->epc);       // PC
      r->r[41] = static_cast<unsigned int>(ef->bad_v_addr);
      r->r[42] = static_cast<unsigned int>(ef->status);
      r->r[43] = static_cast<unsigned int>(ef->cause);
    }
  else
    fill_greg_from_switch_frame(r, switch_ksp);
}

#endif
