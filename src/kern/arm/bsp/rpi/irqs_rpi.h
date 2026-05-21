#pragma once

#include <irq_mgr.h>

namespace Arm_rpi {
  Irq_mgr *create_irq_mgr_bcm(bool primary);
  Irq_mgr *create_irq_mgr_bcm2836(bool primary);
  Irq_mgr *create_irq_mgr_gic(bool primary);
}
