#pragma once

#include "timeout.h"

class Timeslice_timeout : public Timeout
{
public:
  Timeslice_timeout(Cpu_number cpu);

private:
  bool expired() override;
};

