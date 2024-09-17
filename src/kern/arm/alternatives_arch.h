#pragma once

#define ARCH_ALTERNATIVE_ASM_GOTO(probe, no) \
  ALTERNATIVE_INSN("b %l[no]", "nop") : : [alt_probe] "i"(probe) : : no

