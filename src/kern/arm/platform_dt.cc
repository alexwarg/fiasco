
#include <platform_dt.h>
#include <device_tree.h>
#include <dt-arm.h>
#include <timer_arm_generic.h>
#include <alternatives.h>
#include <globalconfig.h>

#include <string.h>

#ifdef CONFIG_ARM_PSCI
#include <psci.h>
#endif

#ifdef CONFIG_DT
#include <pic-gic-dt.h>
#endif


static void init_timer_irqs(Device_tree::Dt &dt)
{
  static char const * const compat[] = {
    "arm,armv8-timer", "arm,armv7-timer", "arm,cortex-a15-timer",
  };
  Device_tree::Node n = dt.node_by_compatible_list(compat);
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

static void init_psci_method(Device_tree::Dt &dt [[maxbe_unused]])
{
#ifdef CONFIG_ARM_PSCI_DYN
  static char const * const compat[] = {
    "arm,psci-1.0", "arm,psci-0.2", "arm,psci",
  };
  Device_tree::Node n = dt.node_by_compatible_list(compat);
  if (!n.is_valid() || !n.is_enabled())
    return;

  char const *method = n.get_prop_str("method");
  if (!method)
    return;

  if (strcmp(method, "hvc") == 0)
    Psci::_psci_use_hvc = true;
  else if (strcmp(method, "smc") == 0)
    Psci::_psci_use_hvc = false;
  else
    return;

  Alternative_insn::init();
#endif
}

void
Platform_dt::init()
{
  if (!Device_tree::dt.valid())
    return;

  init_timer_irqs(Device_tree::dt);
  init_psci_method(Device_tree::dt);
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
