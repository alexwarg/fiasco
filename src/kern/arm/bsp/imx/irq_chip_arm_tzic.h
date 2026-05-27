#pragma once

#include "irq_mgr.h"

namespace Arm_imx_tzic {
  Irq_mgr *create_irq_mgr(bool primary);
}
