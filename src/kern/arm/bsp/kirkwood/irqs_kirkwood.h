#pragma once

#include <irq_mgr.h>

namespace Arm_kirkwood {
  Irq_mgr *create_irq_mgr(bool primary);
}
