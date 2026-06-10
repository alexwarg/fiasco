
#include <alternatives.h>
#include <cstdio>

#include <cstring>
#include "boot_info.h"
#include "mem_unit.h"

inline void fill_nops(Unsigned8 *insn, Unsigned8 *insn_end)
{
  // Replace "disabled instructions" by NOPs. Taken from Intel SDM.
  // 32-bit variants are equivalent to 64-bit variants except s/rax/eax/
  while (insn < insn_end)
    {
      int l = insn_end - insn;
      if (l >= 8)
        {
          // nop dword ptr [rax + rax*1 + 0]
          insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x84; insn[3] = 0x00;
          insn[4] = 0x00; insn[5] = 0x00; insn[6] = 0x00; insn[7] = 0x00;
          insn += 8;
        }
      else if (l >= 7)
        {
          // nop dword ptr [rax + 0]
          insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x80; insn[3] = 0x00;
          insn[4] = 0x00; insn[5] = 0x00; insn[6] = 0x00;
          insn += 7;
        }
      else if (l >= 6)
        {
          // nop word ptr [rax + rax*1 + 0]
          insn[0] = 0x66; insn[1] = 0x0f; insn[2] = 0x1f; insn[3] = 0x44;
          insn[4] = 0x00; insn[5] = 0x00;
          insn += 6;
        }
      else if (l >= 5)
        {
          // nop dword ptr [rax + rax*1 + 0]
          insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x44; insn[3] = 0x00;
          insn[4] = 0x00;
          insn += 5;
        }
      else if (l >= 4)
        {
          // nop dword ptr [rax + 0]
          insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x40; insn[3] = 0x00;
          insn += 4;
        }
      else if (l >= 3)
        {
          // nop dword ptr [rax]
          insn[0] = 0x0f; insn[1] = 0x1f; insn[2] = 0x00;
          insn += 3;
        }
      else if (l >= 2)
        {
          // xchg ax,ax
          insn[0] = 0x66; insn[1] = 0x90;
          insn += 2;
        }
      else
        {
          // nop
          insn[0] = 0x90;
          insn += 1;
        }
    }
}

inline
void
Alternative_insn_entry::enable() const
{
  auto *insn = static_cast<Unsigned8 *>(disabled_insn());
  if (Alternative_insn::Debug)
    printf("  replace insn at %p/%d with",
        static_cast<void *>(insn), len);

  if (this->enabled != 0)
    {
        if (Alternative_insn::Debug)
          printf(" %p/%d", enabled_insn(), rlen);

      // Replace "disabled instructions" by "enabled instructions".
      auto *enabled_insn = static_cast<Unsigned8 const *>(this->enabled_insn());
      memcpy(insn, enabled_insn, rlen);
    }

  if (len > rlen)
    fill_nops(insn + rlen, insn + len);

  if (Alternative_insn::Debug)
    {
      if (len > rlen)
        printf(" + nops %d\n", len - rlen);
      else
        printf("\n");
    }

  Mem_unit::make_coherent_to_pou(insn, len);
}

void
Alternative_insn::init()
{
  extern Alternative_insn_entry const _alt_insns_begin[];
  extern Alternative_insn_entry const _alt_insns_end[];

  if (Debug)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  for (auto const *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        i->enable();
    }

  Boot_info::reset_checksum_ro();
}

