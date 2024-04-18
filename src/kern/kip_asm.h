#pragma once

#include <globalconfig.h>

#ifdef CONFIG_BIT32
#define OFS_KIP_CLOCK 0xA0
#endif
#ifdef CONFIG_BIT64
#define OFS_KIP_CLOCK 0x140
#endif

#define OFS_KIP_CODE_READ_US 0x900
#define OFS_KIP_CODE_READ_NS 0x980
#define KIP_OFS_REL(v, p) OFS_ ##v - (p)

#define KIP_CODE_HDR(s, e, o, n) "\n\t878: .byte " #e " - " #s "\n\t.byte " #n " - 878b\n\t.byte " #o "\n\t.byte 0\n\t"

