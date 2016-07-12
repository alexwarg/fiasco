#pragma once

/* feature bits from cpu-mips.cpp */
#define FEATURE_VZ     0x4
#define FEATURE_TLBINV 0x1

.macro ASM_ALTERNATIVE_ORIG_START
819:
.endm

.macro ASM_ALTERNATIVE_PAD index
829\index :
	.skip  -(((899\index\()f - 889\index\()f) - (829\index\()b - 819b)) > 0) * \
		(((899\index\()f - 889\index\()f) - (829\index\()b - 819b))), 0x00
.endm
.macro ASM_ALTERNATIVE_ORIG_END alternatives=1
	.irp idx, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
		.if \idx <= \alternatives
			ASM_ALTERNATIVE_PAD \idx
		.endif
	.endr
839:
.endm

.macro ASM_ALTERNATIVE_ALT_END alt=1
899\alt :
	.popsection
.endm

.macro ASM_ALTERNATIVE_ALT_START alt, feature, mask=0
	.pushsection	.alt_insns, "a"
888:
	.long	819b - 888b
	.long	889\alt\()f  - 888b
	.half	\feature
	.if \mask == 0
		.half	\feature
	.else
		.half	\mask
	.endif
	.byte	839b - 819b
	.byte	899\alt\()f  - 889\alt\()f
	.popsection
	.pushsection	.alt_insn_replacement, "ax"
889\alt :
.endm


.macro ALTERNATIVE_INSN orig_insn, new_insn, feature
	ASM_ALTERNATIVE_ORIG_START
	\orig_insn
	ASM_ALTERNATIVE_ORIG_END
	ASM_ALTERNATIVE_ALT_START	1, \feature
	\new_insn
	ASM_ALTERNATIVE_ALT_END	1
.endm

