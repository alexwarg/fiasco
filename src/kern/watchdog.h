#pragma once

#include "config.h"

#if ! defined(CONFIG_WATCHDOG)
#include "watchdog_noop.h"
#else
namespace Watchdog
{
  typedef void (*Fn)(void);
  extern Fn touch;
  extern Fn enable;
  extern Fn disable;
}

#endif

