#include <gic.h>

#include <cstdio>

#include <panic.h>
#include <globalconfig.h>

Gic *Gic::primary;

#if defined(CONFIG_ARM_EM_TZ)

bool
Gic_x::alloc(Irq_base *irq, Mword pin, bool init)
{
  if ((pin < 32 && irq->chip() == this && irq->pin() == pin)
      || Irq_chip_gen::alloc(irq, pin, init))
    {
      printf("GIC: Switching IRQ %ld to secure\n", pin);
      _dist.setup_tz_pin(pin);
      return true;
    }
  return false;
}

#else // CONFIG_ARM_EM_TZ

bool
Gic_x::alloc(Irq_base *irq, Mword pin, bool init)
{
  // allow local irqs to be allocated on each CPU
  return (pin < 32 && irq->chip() == this && irq->pin() == pin)
         || Irq_chip_gen::alloc(irq, pin, init);
}

#endif // CONFIG_ARM_EM_TZ

