
#include <alternatives.h>
#include <cstdio>

inline void
Alternative_insn_entry::enable() const
{
  void *insn = disabled_insn();
  void const *enabled_insn = this->enabled_insn();
  memcpy(insn, enabled_insn, len);
  Mem_unit::make_coherent_to_pou(insn, len);
}


void
Alternative_insn::init()
{
  extern Alternative_insn_entry const _alt_insns_begin[];
  extern Alternative_insn_entry const _alt_insns_end[];

  if (0)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  for (auto *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        {
          if (0)
            printf("  replace insn at %p/%d\n", static_cast<void *>(i->disabled_insn()), i->len);
          i->enable();
        }
    }

  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}

