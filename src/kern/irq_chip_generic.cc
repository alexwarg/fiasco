
#include "irq_chip_generic.h"

#include <cstring>

#include "boot_alloc.h"
#include "mem.h"

void
Irq_chip_gen::init(unsigned nirqs)
{
  _nirqs = nirqs;
  _irqs = Boot_alloced::allocate<Irq_base *>(nirqs);
  memset(_irqs, 0, sizeof(Irq_base*) * nirqs);
}

Irq_base *
Irq_chip_gen::irq(Mword pin) const
{
  if (pin >= _nirqs)
    return nullptr;

  return _irqs[pin];
}

bool
Irq_chip_gen::alloc(Irq_base *irq, Mword pin, bool init)
{
  if (pin >= _nirqs)
    return false;

  if (_irqs[pin])
    return false;

  _irqs[pin] = irq;
  bind(irq, pin, !init);
  return true;
}

void
Irq_chip_gen::unbind(Irq_base *irq)
{
  mask(irq->pin());
  Mem::barrier();
  _irqs[irq->pin()] = nullptr;
  Irq_chip_icu::unbind(irq);
}

bool
Irq_chip_gen::reserve(Mword pin)
{
  if (pin >= _nirqs)
    return false;

  if (_irqs[pin])
    return false;

  _irqs[pin] = reinterpret_cast<Irq_base*>(1);

  return true;
}
