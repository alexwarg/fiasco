#pragma once

#include <irq_mgr.h>

namespace Arm_pxa_sa {
  Irq_mgr *create_irq_mgr(bool primary);
}
