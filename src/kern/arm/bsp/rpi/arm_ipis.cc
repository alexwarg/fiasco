
#include <arm_ipis.h>
#include <globalconfig.h>

#if defined (CONFIG_ARM_GIC)
static Arm_ipis::Ipis _arm_ipis;
#endif // CONFIG_ARM_GIC
void Arm_ipis::init_per_cpu(Cpu_number, bool)
{}
