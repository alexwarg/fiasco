#pragma once

#include <irq_mgr.h>

namespace Arm_s3c2410 {
  Irq_mgr *create_irq_mgr(bool primary);
}
