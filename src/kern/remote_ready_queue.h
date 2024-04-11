#pragma once

#include <queue_item.h>
#include <queue.h>
#include <context_base.h>
#include <lock_guard.h>

class Remote_ready_queue : public Queue
{
public:
  template<typename CONTEXT>
  CONTEXT *dequeue_first()
  {
    auto guard = lock_guard(q_lock());
    Queue_item *qi = first();
    if (!qi)
      return nullptr;

    check (dequeue(qi));
    return context_of(qi);
  }

  template<typename S, typename CONTEXT>
  bool handle_requests(CONTEXT *curr, CONTEXT **mq)
  {
    bool resched = false;
    for (;;)
      {
        CONTEXT *c = dequeue_first<CONTEXT>();
        if (!c)
          return resched;

        resched |= S::handle_remote_request(c, mq, curr);
      }
  }

};

