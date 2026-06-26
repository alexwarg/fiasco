
#include "irq_entry.h"

#include "types.h"
#include "mem_unit.h"
#include <panic.h>

extern "C" void irq_handler();

void irq_handler()
{ panic("Invalid IRQ handler."); }



extern Unsigned32 __irq_handler_b_irq[1];

void
Arm_irqs::set_irq_handler(void (*irq_handler)())
{
  auto diff = reinterpret_cast<Unsigned32 *>(irq_handler) - &__irq_handler_b_irq[0];
  // b imm26 (128 MiB offset)
  __irq_handler_b_irq[0] = 0x14000000 | (diff & 0x3ffffff);
  Mem_unit::flush_cache(__irq_handler_b_irq, __irq_handler_b_irq + 1);
}

