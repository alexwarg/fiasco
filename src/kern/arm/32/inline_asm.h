#pragma once

#include <std_macros.h>

// Macro to force 32-bit instructions, even on Thumb builds.
#ifdef __thumb__
#  define FIASCO_ASM_INST32(inst)  inst ".w"
#  define FIASCO_ARM_FPTR_REG "r7"
#  define FIASCO_ARM_CLOBBER_xFPTR "r11"
#  define FIASCO_ARM_JMP_LABEL(x) FIASCO_STRINGIFY((x + 1))

#else
#  define FIASCO_ASM_INST32(inst)  inst
#  define FIASCO_ARM_FPTR_REG "fp"
#  define FIASCO_ARM_CLOBBER_xFPTR "r7"
#  define FIASCO_ARM_JMP_LABEL(x) FIASCO_STRINGIFY(x)
#endif

