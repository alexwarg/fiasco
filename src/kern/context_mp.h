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
#include <drq_log.h>

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
      if (_s.load(cxx::memory_order_relaxed) != Not_running)
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
      if (_s.load(cxx::memory_order_relaxed) != Not_running)
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
    explicit operator bool () const { return _s.load(cxx::memory_order_relaxed) != Not_running; }
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

public:
  void handle_lock_holder_preemption()
  {
    _running_under_lock.preempt();
  }

protected:
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

  bool do_enqueue_drq(Drq *rq)
  {
    Cpu_number cpu = access_once(&_ctxt()->_home_cpu);
    Cpu_number current_cpu = ::current_cpu();

    if (EXPECT_FALSE(cpu == current_cpu))
      return _ctxt()->do_drq(rq);

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

        ipi = pending_rqq_do_enqueue(&q);
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

  /**
   * Dequeue given DRQ from DRQ queue of this context, must be on current or
   * offline CPU, update the context's DRQ ready state and execute the DRQ.
   *
   * \param rq           DRQ to execute.
   * \param offline_cpu  Whether home CPU of context is an offline CPU.
   * \pre The context must be either on the current CPU or on an offline CPU:
   *      `home_cpu() == current_cpu() || offline_cpu`
   * \pre if (offline_cpu) _pending_rqq.cpu(home_cpu()).q_lock() must be held.
   *
   * \return True if re-scheduling is needed (ready queue has changed),
   *         false if not.
   *
   * \post The DRQ function might be preemptible for local DRQ execution, i.e.
   *       `home_cpu() == current_cpu()`, in that case the home CPU can change.
   */
  bool _deq_exec_drq(Drq *rq, bool offline_cpu = false)
  {
    CONTEXT *self = _ctxt();
    if (!self->_drq_q.dequeue(rq))
      return false; // already handled

    if (!self->drq_pending() && EXPECT_FALSE(self->state.has(Thread_drq_ready)))
      self->state.del_dirty(Thread_drq_ready);

    return self->do_drq(rq, offline_cpu);
  }

  static void take_cpu_online(Cpu_number cpu)
  {
    Cpu::cpus.cpu(cpu).set_online(true);
    Rcu::leave_idle(cpu);
  }

};

