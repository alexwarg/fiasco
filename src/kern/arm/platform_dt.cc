
#include <platform_dt.h>
#include <device_tree.h>
#include <dt-arm.h>
#include <timer_arm_generic.h>
#include <globalconfig.h>

#ifdef CONFIG_DT
#include <pic-gic-dt.h>
#endif

void
Platform_dt::init()
{
  if (!Device_tree::dt.valid())
    return;

  static char const * const compat[] = {
    "arm,armv8-timer", "arm,armv7-timer", "arm,cortex-a15-timer",
  };
  Device_tree::Node n = Device_tree::dt.node_by_compatible_list(compat);
  if (!n.is_valid())
    return;

  unsigned irq;

  irq = Dt_arm::get_gic_irq(n, "phys");
  if (irq == ~0u) irq = Dt_arm::get_gic_irq(n, 1u);
  if (irq != ~0u) Timer_generic_timer::_irq_phys = irq;

  irq = Dt_arm::get_gic_irq(n, "virt");
  if (irq == ~0u) irq = Dt_arm::get_gic_irq(n, 2u);
  if (irq != ~0u) Timer_generic_timer::_irq_virt = irq;

  irq = Dt_arm::get_gic_irq(n, "hyp-phys");
  if (irq == ~0u) irq = Dt_arm::get_gic_irq(n, 3u);
  if (irq != ~0u) Timer_generic_timer::_irq_hyp = irq;

  irq = Dt_arm::get_gic_irq(n, "sec-phys");
  if (irq == ~0u) irq = Dt_arm::get_gic_irq(n, 0u);
  if (irq != ~0u) Timer_generic_timer::_irq_secure_hyp = irq;
}

int
Platform_dt::init_irqs_dt()
{
  if (!Device_tree::dt.valid())
    return -1;
#ifdef CONFIG_DT
  return Pic_gic_dt::init();
#endif
}
