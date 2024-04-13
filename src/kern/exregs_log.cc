
#include <exregs_log.h>

#include <cstdio>
#include "string_buffer.h"
#include <thread.h>

void
Log_thread_exregs::print(String_buffer *buf) const
{
  buf->printf("D=%lx ip=%lx sp=%lx op=%s%s%s",
              id, ip, sp,
              op & Thread::Exr_cancel ? "Cancel" : "",
              ((op & (Thread::Exr_cancel | Thread::Exr_trigger_exception))
               == (Thread::Exr_cancel | Thread::Exr_trigger_exception))
               ? ","
               : ((op & (Thread::Exr_cancel | Thread::Exr_trigger_exception))
                  == 0 ? "0" : "") ,
              op & Thread::Exr_trigger_exception ? "TrExc" : "");
}
