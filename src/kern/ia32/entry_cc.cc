#include <thread.h>

extern "C" FIASCO_FASTCALL
void thread_restore_exc_state();

void
thread_restore_exc_state()
{
  current_thread()->restore_exc_state();
}


