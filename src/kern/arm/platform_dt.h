#pragma once

#include <platform_generic.h>

class Platform_dt : public Platform_base
{
public:
  void init() override;
  int init_irqs_dt();
};
