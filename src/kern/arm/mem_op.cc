#include "mem_op.h"

#include "context.h"
#include "entry_frame.h"
#include "globals.h"
#include "mem.h"
#include "mem_space.h"
#include "mem_unit.h"
#include "outer_cache.h"
#include "space.h"
#include "warn.h"

void
Mem_op::l1_inv_dcache(Address start, Address end)
{
  Mword s = Mem_unit::dcache_line_size();
  Mword m = s - 1;
  if (start & m)
    {
      Mem_unit::flush_dcache((void *)start, (void *)start);
      start += s;
      start &= ~m;
    }
  if (end & m)
    {
      Mem_unit::flush_dcache((void *)end, (void *)end);
      end &= ~m;
    }

  if (start < end)
    Mem_unit::inv_dcache((void *)start, (void *)end);
}

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

#ifndef CONFIG_CPU_VIRT

void
Mem_op::arm_mem_access(Mword *r)
{
  Address  a = r[1];
  unsigned w = r[2];

  if (w > 2)
    return;

  if (!current()->space()->is_user_memory(a, 1 << w))
    return;

  jmp_buf pf_recovery;
  int e;

  if ((e = setjmp(pf_recovery)) == 0)
    {
      current()->recover_jmp_buf(&pf_recovery);

      switch (r[0])
	{
	case Op_mem_read_data:
	  switch (w)
	    {
	    case 0:
	      r[3] = *(unsigned char *)a;
	      break;
	    case 1:
	      r[3] = *(unsigned short *)a;
	      break;
	    case 2:
	      r[3] = *(unsigned int *)a;
	      break;
	    default:
	      break;
	    };
	  break;

	case Op_mem_write_data:
	  switch (w)
	    {
	    case 0:
	      *(unsigned char *)a = r[3];
	      break;
	    case 1:
	      *(unsigned short *)a = r[3];
	      break;
	    case 2:
	      *(unsigned int *)a = r[3];
	      break;
	    default:
	      break;
	    };
	  break;

	default:
	  break;
	};
    }
  else
    WARN("Unresolved memory access, skipping\n");

  current()->recover_jmp_buf(0);
}

extern "C" void sys_arm_mem_op();
extern "C" void sys_arm_mem_op()
{
  Entry_frame *e = current()->regs();
  if (EXPECT_FALSE(e->r[0] & 0x10))
    Mem_op::arm_mem_access(e->r);
  else
    Mem_op::arm_mem_cache_maint(e->r[0], (void *)e->r[1], (void *)e->r[2]);
}

#else // CONFIG_CPU_VIRT

extern "C" void sys_arm_mem_op();
extern "C" void sys_arm_mem_op()
{
  Entry_frame *e = current()->regs();
  Mem_op::arm_mem_cache_maint(e->r[0], (void *)e->r[1], (void *)e->r[2]);
}

#endif // CONFIG_CPU_VIRT
