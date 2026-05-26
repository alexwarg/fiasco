
#include <jdb.h>
#include <thread.h>

Thread *
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *c = get_entry_frame(cpu);

  return static_cast<Thread*>(context_of(c));
}

