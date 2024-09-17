
#pragma once

#define ALTERNATIVE_INSN_ENABLED_NOP(disabled_insn)           \
        "819:                                           \n\t" \
        disabled_insn                                  "\n\t" \
        "829:                                           \n\t" \
        ".pushsection .alt_insns, \"a?\"                \n\t" \
        "888:                                           \n\t" \
        ASM_ALTERNATIVE_ENTRY_PTR                      "\n\t" \
        ".4byte 819b - 888b                             \n\t" \
        ".4byte 0                                       \n\t" \
        ".byte 829b - 819b                              \n\t" \
        ".popsection                                    \n\t"

#define ARCH_ALTERNATIVE_ASM_GOTO(probe, no) \
  ALTERNATIVE_INSN_ENABLED_NOP(".byte 0xe9; .4byte %l[no] - 1f; 1:") : : [alt_probe] "i"(probe) : : no

