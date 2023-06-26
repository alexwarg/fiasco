#include "mem_op.h"
#include "mem_unit.h"
#include <context.h>

void
Mem_op::arm_mem_cache_maint(int op, void const *start, void const *end)
{
  if (EXPECT_FALSE(start > end))
    return;

  Context *c = current();

  c->kernel_mem_op.set_ignore(true);
  __arm_mem_cache_maint(op, start, end);
  c->kernel_mem_op.set_ignore(false);
}

void
Mem_op::inv_icache(Address start, Address end)
{
  if (Address(end) - Address(start) > 0x2000)
    asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
  else
    {
      Mword s = Mem_unit::icache_line_size();
      for (start &= ~(s - 1);
           start < end; start += s)
        asm volatile("mcr p15, 0, %0, c7, c5, 1" : : "r" (start));
    }
}
