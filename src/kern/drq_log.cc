
#include <drq_log.h>
#include <kobject_dbg.h>

void
Drq_log::print(String_buffer *buf) const
{
  static char const *const _types[] =
    { "send", "do send", "do request", "send reply", "do reply", "done" };

  char const *t = "unk";
  if ((unsigned)type < sizeof(_types)/sizeof(_types[0]))
    t = _types[(unsigned)type];

  buf->printf("%s(%s) rq=%p/%lx to ctxt=%lx/%p (func=%p) cpu=%u",
      t, wait ? "wait" : "no-wait", rq, Kobject_dbg::pointer_to_id(rq),
      Kobject_dbg::pointer_to_id(thread),
      thread, func, cxx::int_value<Cpu_number>(target_cpu));
}

