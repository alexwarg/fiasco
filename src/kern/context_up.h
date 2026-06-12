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
  bool running_on_different_cpu() const
  { return false; }

  bool need_help(cxx::atomic<Mword> const *, Mword) const
  { return true; }

  void pending_rqq_enqueue()
  {
    if (!Cpu::online(_ctxt()->home_cpu()))
      _ctxt()->handle_remote_state_change();
  }

public:
  void handle_lock_holder_preemption()
  {}

  void dec_lock_cnt()
  {
    _ctxt()->_lock_cnt.sub_fetch(1, cxx::memory_order_relaxed);
  }

  bool do_enqueue_drq(Drq *rq)
  {
    return _ctxt()->do_drq(rq);
  }

  void rcu_wait()
  {
    // The UP case does not need to block for the next grace period, because
    // the CPU is always in a quiescent state when the interrupts where enabled
  }
};
