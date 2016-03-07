#pragma once

#include <drq.h>
#include <drq_queue.h>
#include <context_base.h>
#include <per_cpu_data.h>
#include <logdefs.h>
#include <globalconfig.h>
#include <cassert>

#ifdef CONFIG_JDB
#include <drq_log.h>
#endif

class Context_drq_base
{
protected:
  struct Kernel_drq : Drq { Context *src; };

  Drq _drq;
  Drq_queue _drq_q;

  static Per_cpu<Kernel_drq> _kernel_drq;
};

template<typename CTXT>
class Context_drq_x : public Context_drq_base
{
private:
  CTXT *_this() noexcept
  { return static_cast<CTXT *>(this); }

  CTXT const *_this() const noexcept
  { return static_cast<CTXT const *>(this); }

  using Context = CTXT;

public:
  bool execute_drq(Drq *r, Drq_queue::Drop_mode drop, bool local)
  {
    if (r->context() == _this())
      return execute_drq_reply(r);
    else
      return execute_drq_request(r, drop, local);
  }

  bool enqueue_drq(Drq *rq)
  {
    assert (cpu_lock.test());

    LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
        l->type = rq->context() == _this()
                                   ? Drq_log::Type::Send_reply
                                   : Drq_log::Type::Do_send;
        l->func = (void*)rq->func;
        l->thread = _this();
        l->target_cpu = _this()->home_cpu();
        l->wait = 0;
        l->rq = rq;
    );

    return _this()->do_enqueue_drq(rq);
  }

  /**
   * \brief Initiate a DRQ for the context.
   * \pre \a src must be the currently running context.
   * \param src the source of the DRQ (the context who initiates the DRQ).
   * \param func the DRQ handler.
   * \param arg the argument for the DRQ handler.
   *
   * DRQs are requests that any context can queue to any other context. DRQs are
   * the basic mechanism to initiate actions on remote CPUs in an MP system,
   * however, are also allowed locally.
   * DRQ handlers of pending DRQs are executed by Context::handle_drq() in the
   * context of the target context. Context::handle_drq() is basically called
   * after switching to a context in Context::switch_exec_locked().
   *
   * This function enqueues a DRQ and blocks the current context for a reply DRQ.
   */
  void drq(Drq *drq, Drq::Request_func *func, void *arg,
           Drq::Wait_mode wait = Drq::Wait)
  {
    Context *cur = current();
    LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
        l->type = Drq_log::Type::Send;
        l->rq = drq;
        l->func = (void*)func;
        l->thread = _this();
        l->target_cpu = _this()->home_cpu();
        l->wait = wait;
    );

    assert (!(wait == Drq::Wait && cur->state.has(Thread_drq_ready))
            || cur->home_cpu() == _this()->home_cpu());
    assert (!((wait == Drq::Wait || drq == &_this()->_drq)
              && cur->state.has(Thread_drq_wait)));
    assert (!drq->queued());

    drq->func  = func;
    drq->arg   = arg;
    if (wait == Drq::Wait)
      cur->state.add(Thread_drq_wait);

    _this()->enqueue_drq(drq);

    while (wait == Drq::Wait && cur->state.dirty() & Thread_drq_wait)
      {
        cur->state.del(Thread_ready_mask);
        cur->schedule();
      }

    LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
        l->type = Drq_log::Type::Done;
        l->rq = drq;
        l->func = (void*)func;
        l->thread = _this();
        l->target_cpu = _this()->home_cpu();
        l->wait = wait;
    );
  }

  void drq(Drq::Request_func *func, void *arg,
           Drq::Wait_mode wait = Drq::Wait)
  { return drq(&static_cast<Context *>(current())->_drq, func, arg, wait); }

  bool kernel_context_drq(Drq::Request_func *func, void *arg);

private:
  bool execute_drq_reply(Drq *r)
  {
    (void)r;
    LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
        l->type = Drq_log::Type::Do_reply;
        l->rq = r;
        l->func = (void*)r->func;
        l->thread = r->context();
        l->target_cpu = current_cpu();
        l->wait = 0;
    );
    _this()->state.change_dirty(~Thread_drq_wait, Thread_ready);
    _this()->handle_remote_state_change();
    return !_this()->state.has(Thread_ready_mask);
  }

  bool execute_drq_request(Drq *r, Drq_queue::Drop_mode drop, bool local)
  {
    LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
        l->type = Drq_log::Type::Do_request;
        l->rq = r;
        l->func = (void*)r->func;
        l->thread = r->context();
        l->target_cpu = current_cpu();
        l->wait = 0;
    );

    Drq::Result answer = Drq::done();
    if (EXPECT_TRUE(drop == Drq_queue::No_drop && r->func))
      {
        _this()->handle_remote_state_change();
        answer = r->func(r, _this(), r->arg);
      }
    else if (EXPECT_FALSE(drop == Drq_queue::Drop))
      // flag DRQ abort for requester
      r->arg = (void*)-1;

    bool need_resched = answer.need_resched();

    // enqueue answer
    if (!(answer.no_answer()))
      {
        Context *c = r->context();
        if (local)
          c->state.change_dirty(~Thread_drq_wait, Thread_ready);
        else
          need_resched |= c->enqueue_drq(r);
      }
    return need_resched;
  }
};


template<typename C>
bool
Context_drq_x<C>::kernel_context_drq(Drq::Request_func *func, void *arg)
{
  if (EXPECT_TRUE(_this()->home_cpu() == _this()->get_current_cpu()))
    _this()->update_ready_list();

  Context *kc = _this()->kernel_context(_this()->get_current_cpu());
  if (current() == kc)
    return func(0, kc, arg).need_resched();

  Kernel_drq *mdrq = new (&_kernel_drq.cpu(_this()->get_current_cpu())) Kernel_drq;

  mdrq->src = _this();
  mdrq->func  = func;
  mdrq->arg   = arg;
  kc->_drq_q.enq(mdrq);
  return _this()->schedule_switch_to_locked(kc) != Context::Switch::Ok;
}

