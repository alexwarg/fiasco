#pragma once

#include <irq_mgr.h>

namespace Arm_imx {
  Irq_mgr *create_irq_mgr(bool primary);
}

namespace Arm_imx_icoll {
  Irq_mgr *create_irq_mgr(bool primary);
}
