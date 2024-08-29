#pragma once

#define FIASCO_ARM_SMC_CALL_ASM_OUTPUTS \
    "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3)

#define FIASCO_ARM_SMC_CALL_ASM_INPUTS \
    "0" (r0), "1" (r1), "2" (r2), "3" (r3), \
    "r" (r4), "r" (r5), "r" (r6), "r" (r7)

#define FIASCO_ARM_SMC_CALL_ASM_CLOBBERS \
    "memory"

#define FIASCO_ARM_ASM_REG(n) asm("r" # n)

#define FIASCO_ARM_ARCH_EXTENSION_SEC ".arch_extension sec\n"
#define FIASCO_ARM_ARCH_EXTENSION_NOSEC ".arch_extension nosec\n"

#define FIASCO_ARM_SMC_CALL_ASM_OPERANDS \
    : FIASCO_ARM_SMC_CALL_ASM_OUTPUTS \
    : FIASCO_ARM_SMC_CALL_ASM_INPUTS \
    : FIASCO_ARM_SMC_CALL_ASM_CLOBBERS
