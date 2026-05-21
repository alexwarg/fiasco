#pragma once

#include <irq_mgr.h>

namespace Arm_omap3 {
  Irq_mgr *create_irq_mgr(bool primary);
}
