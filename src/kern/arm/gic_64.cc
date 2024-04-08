
#include "gic.h"

#include "types.h"
#include "mem_unit.h"

void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern Unsigned32 __irq_handler_b_irq[1];
  auto diff = reinterpret_cast<Unsigned32 *>(irq_handler) - &__irq_handler_b_irq[0];
  // b imm26 (128 MB offset)
  __irq_handler_b_irq[0] = 0x14000000 | (diff & 0x3ffffff);
  Mem_unit::flush_cache(__irq_handler_b_irq, __irq_handler_b_irq + 1);
}

