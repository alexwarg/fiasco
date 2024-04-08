
#include "gic.h"
#include "globalconfig.h"

#if defined (CONFIG_CPU_VIRT)

void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern void (*__irq_handler_irq)();
  __irq_handler_irq = irq_handler;
}

#else // CONFIG_CPU_VIRT

void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern void (*__irq_handler_fiq)();
  extern void (*__irq_handler_irq)();
  __irq_handler_fiq = irq_handler;
  __irq_handler_irq = irq_handler;
}

#endif // CONFIG_CPU_VIRT

