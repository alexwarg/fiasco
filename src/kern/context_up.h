#pragma once

#include <logdefs.h>
#include <drq_queue.h>
#include <drq.h>
#include <drq_log.h>

template<typename CONTEXT>
class Context_up_x
{
private:
  CONTEXT *_ctxt() noexcept
  { return static_cast<CONTEXT *>(this); }

  CONTEXT const *_ctxt() const noexcept
  { return static_cast<CONTEXT const *>(this); }

protected:
  void handle_lock_holder_preemption()
  {}

  bool running_on_different_cpu() const
  { return false; }

  bool need_help(Mword const *, Mword) const
  { return true; }

  void pending_rqq_enqueue()
  {
    if (!Cpu::online(_ctxt()->home_cpu()))
      _ctxt()->handle_remote_state_change();
  }

public:
  void dec_lock_cnt()
  {
    _ctxt()->_lock_cnt.sub_fetch(1, cxx::memory_order_relaxed);
  }

  bool enqueue_drq(Drq *rq)
  {
    assert (cpu_lock.test());

    CONTEXT *self = _ctxt();

    LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
        l->type = rq->context() == self
                                   ? Drq_log::Type::Send_reply
                                   : Drq_log::Type::Do_send;
        l->func = (void*)rq->func;
        l->thread = self;
        l->target_cpu = self->home_cpu();
        l->wait = 0;
        l->rq = rq;
    );

    bool do_sched = self->execute_drq(rq, Drq_queue::No_drop, true);
    if (   access_once(&self->_home_cpu) == current_cpu()
        && self->state.has(Thread_ready_mask) && !self->in_ready_list())
      {
        Sched_context::rq.current().ready_enqueue(self->sched());
        return true;
      }
    return do_sched;
  }

  void rcu_wait()
  {
    // The UP case does not need to block for the next grace period, because
    // the CPU is always in a quiescent state when the interrupts where enabled
  }
};
