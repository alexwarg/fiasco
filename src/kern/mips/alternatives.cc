
#include <alternatives.h>

#include <cstdio>
#include <cstring>
#include "asm_mips.h"


inline void
Alternative_insn::replace() const
{
  Unsigned32 *orig_insn = orig_code();
  Unsigned32 const *alt_insn = this->alt_insn();
  memcpy(orig_insn, alt_insn, r_len);
  if (r_len < total_len)
    memset((char *)orig_insn + r_len, 0, total_len - r_len);

  // sync insn cache, this code does not use synci_step but uses 4byte steps
  for (unsigned i = 0; i <= total_len / 4; ++i)
    asm volatile ("synci %0" : : "R"(orig_insn[i]));

  Mword dummy;
  asm volatile ("sync; " ASM_LA " %0, 1f; jr.hb %0; nop; 1: nop;" : "=r"(dummy));
}

void
Alternative_insn::handle_alternatives(unsigned features)
{
  extern Alternative_insn const _alt_insns_begin[];
  extern Alternative_insn const _alt_insns_end[];

  if (0)
    printf("patching alternative instructions for feature: %x\n", features);

  for (auto *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if ((features & i->mask) == i->feature)
        {
          if (0)
            printf("  replace insn at %p %08x -> %08x\n",
                   i->orig_code(), *i->orig_code(), *i->alt_insn());
          i->replace();
        }
    }

  // finally clear all instruction hazards
  asm volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      ".set noat\n\t"
      "sync\n\t"
      ASM_LA " $at, 1f\n\t"
      "jr.hb $at\n\t"
      "  nop\n\t"
      "1:\n\t"
      ".set pop"
      );
}

