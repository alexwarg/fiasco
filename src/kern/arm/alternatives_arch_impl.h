#pragma once

#include <alternatives.h>
#include <cstdio>
#include <mem_unit.h>

inline void
Alternative_insn_entry::enable() const
{
  void *insn = disabled_insn();
  void const *enabled_insn = this->enabled_insn();
  memcpy(insn, enabled_insn, len);
  Mem_unit::make_coherent_to_pou(insn, len);
}

inline
void
Alternative_insn::patch_finish()
{
  // Mem::dsb() already included in Mem_unit::make_coherent_to_pou()
  Mem::isb();
}

