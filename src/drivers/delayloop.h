
#pragma once

#include "std_macros.h"
#include "initcalls.h"

class Delay
{
public:
  static void init() FIASCO_INIT;
  static void delay(unsigned ms);
};

