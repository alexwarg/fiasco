#pragma once

#define FIASCO_ARM_SMC_CALL_ASM_OPERANDS \
    : "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3) \
    : "0" (r0), "1" (r1), "2" (r2), "3" (r3), \
      "r" (r4), "r" (r5), "r" (r6), "r" (r7) \
    : "memory"

#define FIASCO_ARM_ASM_REG(n) asm("r" # n)

#define FIASCO_ARM_ARCH_EXTENSION_SEC ".arch_extension sec\n"

