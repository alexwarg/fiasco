#pragma once

#include <globalconfig.h>

#ifdef CONFIG_PF_OMAP3_OMAP35XEVM
#define TARGET_NAME "OMAP35xEVM"
#endif

#ifdef CONFIG_PF_OMAP3_BEAGLEBOARD
#define TARGET_NAME "Beagleboard"
#endif

#ifdef CONFIG_PF_OMAP3_AM33XX
#define TARGET_NAME "AM33xx"
#endif

#ifdef CONFIG_PF_OMAP4_PANDABOARD
#define TARGET_NAME "Pandaboard"
#endif

#ifdef CONFIG_PF_OMAP5_5432EVM
#define TARGET_NAME "OMAP5"
#endif
