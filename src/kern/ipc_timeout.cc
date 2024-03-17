#include "ipc_timeout.h"
#include "thread_state.h"

/**
 * Timeout expiration callback function
 * @return true if reschedule is necessary, false otherwise
 */
bool
IPC_timeout::expired()
{
  Receiver * const _owner = owner();

  Mword ipc_state = _owner->state() & Thread_ipc_mask;
  if (!ipc_state || (ipc_state & Thread_receive_in_progress))
    return false;

  _owner->state_add_dirty(Thread_ready | Thread_timeout);

  // Flag reschedule if owner's priority is higher than the current
  // thread's (own or timeslice-donated) priority.
  return Sched_context::rq.current().deblock(_owner->sched(), current()->sched(), false);
}

