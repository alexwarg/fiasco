#pragma once

#include <context_base.h>
#include <queue_item.h>
#include <cxx/bitfield>

/**
 * \brief Deferred Request.
 *
 * Represents a request that can be queued for each Context
 * and is executed by the target context just after switching to the
 * target context.
 */
class Drq : public Queue_item, public Context_member
{
public:
  struct Result
  {
    unsigned char v;
    CXX_BITFIELD_MEMBER(0, 0, need_resched, v);
    CXX_BITFIELD_MEMBER(1, 1, no_answer, v);
  };

  static Result done()
  { Result r; r.v = 0; return r; }

  static Result no_answer()
  { Result r; r.v = Result::no_answer_bfm_t::Mask; return r; }

  static Result need_resched()
  { Result r; r.v = Result::need_resched_bfm_t::Mask; return r; }

  static Result no_answer_resched()
  {
    Result r;
    r.v = Result::no_answer_bfm_t::Mask | Result::need_resched_bfm_t::Mask;
    return r;
  }

  typedef Result (Request_func)(Drq *, Context *target, void *);
  enum Wait_mode { No_wait = 0, Wait = 1 };
  // enum State { Idle = 0, Handled = 1, Reply_handled = 2 };

  Request_func *func;
  void *arg;
  // State state;
};


