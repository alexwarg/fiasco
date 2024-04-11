#pragma once

#include <remote_ready_queue.h>
#include <cpu_call.h>
#include <mem.h>
#include <lock_guard.h>
#include <context_base.h>
#include <ipi.h>
#include <rcupdate.h>
#include <drq.h>
#include <drq_queue.h>
#include <sched_context.h>
#include <logdefs.h>

#include <cassert>


class Context_mp_base
{
protected:
  friend class Switch_lock;
  friend class Locks_test;

  /**
   * The running-under-lock state machine.
   *
   * This state machine handles state transitions for the
   * Context::_running_under_lock variable to ensure correct multi-
   * processor helping semantics.
   *
   * We use three states:
   * 1. `Not_running` --- The thread is not running on a foreign CPU. However,
   *    it might be running on its home CPU without holding any helping lock.
   * 2. `Trying` --- A thread on a foreign CPU has the intention to start
   *    helping but has yet to evaluate if this thread owns the desired lock.
   * 3. `Running` --- This thread is running on a CPU while usually holding
   *    at least one helping lock.
   */
  class Running_under_lock
  {
    friend class Locks_test; // Locks_test::test_switch_lock_acquire_free()

    // FIXME: could be a byte, but nee cas byte for that
    enum State : Mword
    {
      Not_running = 0,
      Trying      = 1,
      Running     = 2,
    };

    cxx::atomic<State> _s;

  public:
    /**
     * Atomic transition from `Not_running` to `Trying`.
     *
     * \retval true, on success. This means no other thread is currently
     *         helping or trying to help. However, the thread might
     *         currently execute on its home CPU.
     * \retval false, another thread is currently helping or trying to help.
     *         This means the helper shall retry to grab the helping lock.
     *
     * Must be used by a potential helper thread before double checking
     * if this thread owns the desired lock. If yes, then
     * Running_under_lock::help() must be called before helping starts.
     * If no, Running_under_lock::reset() must be called.
     */
    bool try_to_help()
    {
      if (_s.load(cxx::memory_order_relaxed))
        return false; // either running or already trying

      State not_running = Not_running;
      return _s.compare_exchange_strong(not_running, Trying, cxx::memory_order_acquire);
    }

    /**
     * Atomic transition from `Not_running` to `Running`.
     *
     * \pre Must be called by the current thread on its own
     *      `_running_under_lock` member only.
     *
     * \retval true, on success. This means no other thread is currently
     *         helping or trying to help.
     * \retval false, another thread is currently trying to help. This means
     *         the caller must wait before grabbing the first lock until the
     *         potential helper aborted its helping.
     */
    bool try_dispatch()
    {
      if (_s.load(cxx::memory_order_relaxed))
        return false;

      State not_running = Not_running;
      return _s.compare_exchange_strong(not_running, Running, cxx::memory_order_acquire);
    }

    /**
     * Transition from Trying to Running.
     *
     * Before calling this method Running_under_lock::try_to_help() must
     * have returned success.
     */
    void help() { _s.store(Running, cxx::memory_order_relaxed); }

    /**
     * Dirty transition to Not_running.
     *
     * This method is to be used to abort an unsuccessful attempt to help
     * or after clearing the last lock when running on the home CPU.
     */
    void reset()
    {
      _s.store(Not_running, cxx::memory_order_release);
    }

    /**
     * Safe transition from Running to Not_running.
     *
     * This method has to be used to safely mark a thread as not running
     * in the preemption code (after switching away from this thread).
     */
    void preempt()
    {
      if (_s.load(cxx::memory_order_relaxed) == Running)
        _s.store(Not_running, cxx::memory_order_release);
    }

    /// Check the current running under lock state.
    operator bool () const { return _s.load(cxx::memory_order_relaxed); }
  };

  using Pending_rqq = Remote_ready_queue;

  [[gnu::nonnull]]
  bool pending_rqq_do_enqueue(Queue *q)
  {
    if (_pending_rq.queued())
      return false;

    bool ipi = !q->first();
    q->enqueue(&_pending_rq);
    return ipi;
  }

  void handle_lock_holder_preemption()
  {
    _running_under_lock.preempt();
  }

  /**
   * Synchronization variable for multi-processor helping.
   *
   * This variable in combination with the _lock_cnt variable is used to
   * synchronize dispatching of this context during cross-processor helping.
   */
  Running_under_lock _running_under_lock;
  Queue_item _pending_rq;

  static Per_cpu<Pending_rqq> _pending_rqq;

};

template<typename CONTEXT>
class Context_mp_x : public Context_mp_base
{
private:
  friend class Remote_ready_queueu;

  CONTEXT *_ctxt() noexcept
  { return static_cast<CONTEXT *>(this); }

  CONTEXT const *_ctxt() const noexcept
  { return static_cast<CONTEXT const *>(this); }

public:
  void dec_lock_cnt()
  {
    int ncnt = _ctxt()->_lock_cnt.sub_fetch(1, cxx::memory_order_relaxed);
    if (EXPECT_TRUE(ncnt == 0 && _ctxt()->home_cpu() == current_cpu()))
      {
        Mem::mp_wmb();
        _running_under_lock.reset();
      }
  }

  void try_finish_migration()
  {
    if (_ctxt()->state.change_safely(~Thread_finish_migration, 0))
      _ctxt()->finish_migration();
  }

  bool running_on_different_cpu()
  {
    if (   EXPECT_TRUE(_ctxt()->_lock_cnt.load(cxx::memory_order_acquire) == 0)
        && EXPECT_TRUE(!_running_under_lock))
      return false;

    if (EXPECT_FALSE(!_running_under_lock.try_dispatch()))
      return true;

    if (EXPECT_FALSE(_ctxt()->_lock_cnt.load(cxx::memory_order_acquire) == 0))
      _running_under_lock.reset();

    return false;
  }

  bool need_help(Mword const *lock, Mword val)
  {
    if (EXPECT_FALSE(!_running_under_lock.try_to_help()))
      return false;

    // double check if the lock is held by us
    if (EXPECT_TRUE(_ctxt()->_lock_cnt.load(cxx::memory_order_acquire) != 0
                    && access_once(lock) == val))
      {
        _running_under_lock.help();
        return true;
      }

    _running_under_lock.reset();
    return false;
  }

  bool handle_remote_request(CONTEXT **mq, CONTEXT *curr)
  {
    CONTEXT *self = _ctxt();
    assert (self->check_for_current_cpu());

    bool resched = false;

    self->handle_remote_state_change();
    if (EXPECT_FALSE(self->migration_pending()))
      {
        // if the currently executing thread shall be migrated we must defer
        // this until we have handled the whole request queue, otherwise we
        // would miss the remaining requests or execute them on the wrong CPU.
        if (self != curr)
          {
            // we can directly migrate the thread...
            resched |= self->initiate_migration();

            // if migrated away skip the resched test below
            if (access_once(&self->_home_cpu) != curr->get_current_cpu())
              return resched;
          }
        else
          *mq = self;
      }
    else
      self->try_finish_migration();

    if (EXPECT_TRUE(self->drq_pending()))
      {
        if (EXPECT_FALSE(self == curr))
          return self->handle_drq() || resched;
        else
          self->state.add(Thread_drq_ready);
      }

    // here we had no DRQ pending, or made this thread
    // Thread_drq_ready if not currently running
    if (EXPECT_TRUE(self != curr && self->state.has(Thread_ready_mask)))
      {
        Sched_context *cs = (curr->home_cpu() == curr->get_current_cpu())
                          ? curr->sched()
                          : 0;

        return Sched_context::rq.current().deblock(self->sched(), cs) || resched;
      }

    return resched;
  }

  void pending_rqq_enqueue()
  {
    bool ipi = false;
    CONTEXT *self = _ctxt();
    // read cpu again we may've been migrated meanwhile
    Cpu_number cpu = access_once(&self->_home_cpu);
    Queue &q = CONTEXT::_pending_rqq.cpu(cpu);

      {
        auto guard = lock_guard(q.q_lock());

        // migrated between getting the lock and reading the CPU, so the
        // new CPU is responsible for executing our request
        if (access_once(&self->_home_cpu) != cpu)
          return;

        if (!Cpu::online(cpu))
          {
            self->handle_remote_state_change();
            return;
          }

        ipi = pending_rqq_do_enqueue(&q);
      }

    if (ipi)
      Ipi::send(Ipi::Request, current_cpu(), cpu);
  }

  bool enqueue_drq(Drq *rq)
  {
    assert (cpu_lock.test());

    Cpu_number cpu = access_once(&_ctxt()->_home_cpu);
    Cpu_number current_cpu = ::current_cpu();

    LOG_TRACE("DRQ handling", "drq", current(), typename CONTEXT::Drq_log,
        l->type = rq->context() == _ctxt()
                                   ? CONTEXT::Drq_log::Type::Send_reply
                                   : CONTEXT::Drq_log::Type::Do_send;
        l->func = (void*)rq->func;
        l->thread = _ctxt();
        l->target_cpu = cpu;
        l->wait = 0;
        l->rq = rq;
    );

    if (EXPECT_FALSE(cpu == current_cpu))
      return _execute_drq(rq);

    _ctxt()->_drq_q.enq(rq);

    // re-read the cpu number, we may have been migrated. We need to be sure to
    // signal the right CPU that there is work for us.
    cpu = access_once(&_ctxt()->_home_cpu);

    // check if we migrated to the current_cpu, in this case we have to execute
    // the DRQ directly
    if (EXPECT_FALSE(cpu == current_cpu))
      return _deq_exec_drq(rq);

    bool ipi = false;

      {
        Queue &q = CONTEXT::_pending_rqq.cpu(cpu);
        auto guard = lock_guard(q.q_lock());

        // migrated between getting the lock and reading the CPU, so the
        // new CPU is responsible for executing our request
        if (access_once(&_ctxt()->_home_cpu) != cpu)
          return false;

        if (EXPECT_FALSE(!Cpu::online(cpu)))
          return _deq_exec_drq(rq, true);

        if (!_pending_rq.queued())
          {
            if (!q.first())
              ipi = true;

            q.enqueue(&_pending_rq);
          }
      }

    if (ipi)
      Ipi::send(Ipi::Request, current_cpu, cpu);

    return false;
  }

  /**
   * Block and wait for the next grace period.
   */
  void rcu_wait()
  {
    auto guard = lock_guard(cpu_lock);
    _ctxt()->state.change_dirty(~Thread_ready, Thread_waiting);
    Rcu::call(_ctxt(), &CONTEXT::rcu_unblock);
    while (_ctxt()->state.dirty() & Thread_waiting)
      {
        _ctxt()->state.del_dirty(Thread_ready);
        _ctxt()->schedule();
      }
  }

  bool _deq_exec_drq(Drq *rq, bool offline_cpu = false)
  {
    CONTEXT *self = _ctxt();
    if (!self->_drq_q.dequeue(rq))
      return false; // already handled

    if (!self->drq_pending() && EXPECT_FALSE(self->state.has(Thread_drq_ready)))
      self->state.del_dirty(Thread_drq_ready);

    return _execute_drq(rq, offline_cpu);
  }


  static bool take_cpu_offline(Cpu_number cpu, bool drain_rqq = false)
  {
    assert (cpu == current_cpu());
    assert (!Proc::interrupts());

    for (;;)
      {
        auto &q = CONTEXT::_pending_rqq.current();

          {
            auto guard = lock_guard(q.q_lock());

            if (!q.first())
              {
                Cpu::cpus.current().set_online(false);
                break;
              }

            if (!drain_rqq)
              return false;
          }

        // Pending_rqq::handle_requests must be called without the
        // queue lock held.
        Context *migration_q = 0;
        q.handle_requests(current(), &migration_q);
        // assume we run from the idle thread, and the idle thread does
        // never migrate so `migration_q` must be 0
        assert (!migration_q);
      }

    Mem::mp_mb();

    // As the interrupts are disabled (this is acceptable as this function is
    // called during system suspend only), the loop safely drains all the RCU
    // queues of the current CPU without race conditions. And the enter_idle()
    // does safely remove the CPU from the list of active CPUs.
    do
      {
        Rcu::do_pending_work(cpu);
        Proc::pause();
      }
    while (!Rcu::idle(cpu));
    Rcu::enter_idle(cpu);

    Cpu_call::handle_global_requests();

    return true;
  }

  static void take_cpu_online(Cpu_number cpu)
  {
    Cpu::cpus.cpu(cpu).set_online(true);
    Rcu::leave_idle(cpu);
  }

private:
  bool _execute_drq(Drq *rq, bool offline_cpu = false)
  {
    CONTEXT *self = _ctxt();
    bool do_sched = self->execute_drq(rq, Drq_queue::No_drop, true);
    // the DRQ function executed above might be preemptible in the case
    // of local execution
    if (EXPECT_FALSE(!offline_cpu && self->home_cpu() != current_cpu()))
      return false;

    if (!self->in_ready_list() && self->state.has(Thread_ready_mask))
      {
        if (EXPECT_FALSE(offline_cpu))
          Sched_context::rq.cpu(self->home_cpu()).ready_enqueue(self->sched());
        else
          Sched_context::rq.current().ready_enqueue(self->sched());
        return true;
      }

    return do_sched;
  }
};
