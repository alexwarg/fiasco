#include <irq_entry.h>
#include "globalconfig.h"
#include <panic.h>

extern "C" void irq_handler();

void irq_handler()
{ panic("Invalid IRQ handler."); }


extern void (*__irq_handler_irq)();
#if defined (CONFIG_CPU_VIRT)

void
Arm_irqs::set_irq_handler(void (*irq_handler)())
{
  __irq_handler_irq = irq_handler;
}

#else // CONFIG_CPU_VIRT

extern void (*__irq_handler_fiq)();
void
Arm_irqs::set_irq_handler(void (*irq_handler)())
{
  __irq_handler_fiq = irq_handler;
  __irq_handler_irq = irq_handler;
}

#endif // CONFIG_CPU_VIRT

