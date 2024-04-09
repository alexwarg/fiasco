#pragma once

#include <globalconfig.h>

#if defined (CONFIG_PF_RPI_RPI1) || defined (CONFIG_PF_RPI_RPIZW)
#define TARGET_NAME "RPi1 (Broadcom 2835)"
#endif

#ifdef CONFIG_PF_RPI_RPI2
#define TARGET_NAME "RPi2 (Broadcom 2836)"
#endif

#ifdef CONFIG_PF_RPI_RPI3
#define TARGET_NAME "RPi3 (Broadcom 2837)"
#endif

#ifdef CONFIG_PF_RPI_RPI4
#define TARGET_NAME "RPi4 (Broadcom 2711)"
#endif
