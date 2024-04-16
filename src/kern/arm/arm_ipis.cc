
// generic version usful nor normal GIC setups etc.
// If needed a BSP can have it's own arm_ipis.cc file with
// special code.

#include <arm_ipis.h>

static Arm_ipis::Ipis _arm_ipis;

void Arm_ipis::init_per_cpu(Cpu_number, bool)
{}

