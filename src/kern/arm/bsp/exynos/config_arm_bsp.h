#pragma once

#include <globalconfig.h>

#define TARGET_NAME "Samsung Exynos"

namespace Config
{
  enum
  {
#ifdef CONFIG_PF_EXYNOS_UART_NR
    Uart_nr = CONFIG_PF_EXYNOS_UART_NR
#else
    Uart_nr = 0
#endif
  };
}
