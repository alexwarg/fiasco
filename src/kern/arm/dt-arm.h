#pragma once

#include <device_tree.h>

namespace Dt_arm {

unsigned get_gic_irq(Device_tree::Node n, unsigned idx);
unsigned get_gic_irq(Device_tree::Node n, const char *name);

}
