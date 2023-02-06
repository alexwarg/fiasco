#pragma once

#include <drq.h>
#include <queue.h>
#include <lock_guard.h>

#include <cassert>
#include <cpu_lock.h>

/**
 * \brief Queue for deferred requests (Drq).
 *
 * A FIFO queue each Context aggregates to queue incoming Drq's
 * that have to be executed directly after switching to a context.
 */
class Drq_queue : public Queue
{
public:
  void enq(Drq *rq)
  {
    assert(cpu_lock.test());
    auto guard = lock_guard(q_lock());
    enqueue(rq);
  }

  bool dequeue(Drq *drq)
  {
    auto guard = lock_guard(q_lock());
    if (!drq->queued())
      return false;
    return Queue::dequeue(drq);
  }

  template<typename CTXT>
  bool handle_requests(CTXT *ctxt)
  {
    bool need_resched = false;
    while (1)
      {
        Queue_item *qi;
          {
            auto guard = lock_guard(q_lock());
            qi = first();
            if (!qi)
              return need_resched;

            check (Queue::dequeue(qi));
          }

        Drq *r = static_cast<Drq*>(qi);
        need_resched |= ctxt->execute_drq(r, false);
      }
  }
};

