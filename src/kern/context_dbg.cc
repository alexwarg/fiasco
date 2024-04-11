
#include <context_dbg.h>

#include <kobject_dbg.h>
#include <string_buffer.h>
#include <sched_context.h>

void
Tb_entry_ctx_sw::print(String_buffer *buf) const
{
  Context *sctx = 0;
  Mword sctxid = ~0UL;
  Mword dst;
  Mword dst_orig;

  sctx = from_sched->context();
  sctxid = Kobject_dbg::pointer_to_id(sctx);

  dst = Kobject_dbg::pointer_to_id(this->dst);
  dst_orig = Kobject_dbg::pointer_to_id(this->dst_orig);

  if (sctx != ctx())
    buf->printf("(%lx)", sctxid);

  buf->printf(" ==> %lx ", dst);

  if (dst != dst_orig || lock_cnt)
    buf->printf("(");

  if (dst != dst_orig)
    buf->printf("want %lx", dst_orig);

  if (dst != dst_orig && lock_cnt)
    buf->printf(" ");

  if (lock_cnt)
    buf->printf("lck %lu", lock_cnt);

  if (dst != dst_orig || lock_cnt)
    buf->printf(") ");

  buf->printf(" krnl " L4_PTR_FMT " @ " L4_PTR_FMT, kernel_ip, _ip);
}


