#include <alternatives.h>
#include <cstdio>
#include <alternatives_arch_impl.h>

void
Alternative_insn::init()
{
  extern Alternative_insn_entry const _alt_insns_begin[];
  extern Alternative_insn_entry const _alt_insns_end[];

  if (Debug)
    printf("patching alternative instructions\n");

  if (&_alt_insns_begin[0] == &_alt_insns_end[0])
    return;

  bool patched = false;
  for (auto const *i = _alt_insns_begin; i != _alt_insns_end; ++i)
    {
      if (i->probe())
        {
          i->enable();
          patched = true;
        }
    }

  if (patched)
    patch_finish();

  if constexpr (Debug)
    printf("Patching done.\n");
}

