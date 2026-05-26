
#include <jdb.h>
#include <thread.h>
#include <dbg_stack.h>

struct On_dbg_stack
{
  Mword sp;
  On_dbg_stack(Mword sp) : sp(sp) {}
  bool operator () (Cpu_number cpu) const
  {
    Dbg::Dbg_stack const &st = Dbg::dbg_stack.cpu(cpu);
    return sp <= Mword(st.stack_top) 
       && sp >= Mword(st.stack_top) - Dbg::Dbg_stack::Stack_size;
  }
};


Thread*
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(cpu);
  Address sp = (Address) entry_frame;

  // special case since we come from the double fault handler stack
  if (entry_frame->_trapno == 8 && !(entry_frame->cs() & 3))
    sp = entry_frame->sp(); // we can trust esp since it comes from main_tss

  if (foreach_cpu(On_dbg_stack(sp), false))
    return 0;

  if (!Helping_lock::threading_system_active)
    return 0;

  return static_cast<Thread*>(context_of((const void*)sp));
}

