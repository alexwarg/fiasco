
#include <jdb.h>
#include <thread.h>
#include <dbg_stack.h>

Thread*
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(cpu);
  Address sp = (Address) entry_frame;

  // special case since we come from the double fault handler stack
  if (entry_frame->_trapno == 8 && !(entry_frame->cs() & 3))
    sp = entry_frame->sp(); // we can trust esp since it comes from main_tss

  auto on_dbg_stack = [sp](Cpu_number cpu) -> bool
    {
      Dbg::Dbg_stack const &st = Dbg::dbg_stack.cpu(cpu);
      Mword stack_top = reinterpret_cast<Mword>(st.stack_top);
      return sp <= stack_top && sp >= stack_top - Dbg::Dbg_stack::Stack_size;
    };
  if (foreach_cpu(on_dbg_stack, false))
    return 0;

  if (!Helping_lock::threading_system_active)
    return 0;

  return static_cast<Thread*>(context_of((const void*)sp));
}

