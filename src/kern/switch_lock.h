#pragma once

#include "member_offs.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "context.h"
#include "mem.h"

#include <cxx/atomic>
#include <types.h>

#ifdef NO_INSTRUMENT
#undef NO_INSTRUMENT
#endif
#define NO_INSTRUMENT __attribute__((no_instrument_function))

class Context;


/**
 * A lock that implements priority inheritance.
 *
 * The lock uses a validity checker for doing an existence check before the lock
 * is actually acquired. With this mechanism the lock itself may disappear while
 * it is locked (see clear_no_switch_dirty() and switch_dirty()), even if it is
 * under contention. When the lock no longer exists, valid() returns false.
 *
 * The operations lock(), test(), test_and_set(), and test_and_set_dirty() may
 * return #Invalid if the lock does no longer exist.
 *
 * The validity checker is used while acquiring the lock to test if the lock
 * itself exists. We assume that a lock may disappear while we are blocked on
 * it.
 */
class Switch_lock
{
  MEMBER_OFFSET();
  friend class Locks_test;

private:
  // Warning: This lock's member variables must not need a
  // constructor.  Switch_lock instances must assume
  // zero-initialization or be initialized using the initialize()
  // member function.
  // Reason: to avoid overwriting the lock in the thread-ctor
  cxx::atomic<Address> _lock_owner;

public:
  /**
   * The result type of lock operations.
   */
  enum Status
  {
    Not_locked, ///< The lock was formerly not acquired and -- we got it
    Locked,     ///< The lock was already acquired by ourselves
    Invalid     ///< The lock does not exist (is invalid)
  };

  /**
   * Stores the context of the lock for a later switch.
   * (see clear_no_switch_dirty(), switch_dirty())
   */
  struct Lock_context
  {
    Context *owner;
  };

  /**
   * Test if the lock is valid.
   *
   * \return true if the lock really exists, false if not.
   */
  bool NO_INSTRUMENT valid() const
  { return (_lock_owner.load(cxx::memory_order_relaxed) & 1) == 0; }

  /**
   * Initialize Switch_lock.
   *
   * Call this function if you cannot guarantee that your Switch_lock instance is
   * allocated from zero-initialized memory.
   */
  void NO_INSTRUMENT initialize()
  {
    _lock_owner.store(0, cxx::memory_order_release);
  }

  /**
   * Lock owner.
   *
   * \return current owner of the lock. 0 if there is no owner.
   */
  Context * NO_INSTRUMENT lock_owner() const
  {
    return reinterpret_cast<Context*>(_lock_owner.load(cxx::memory_order_relaxed) & ~1UL);
  }

  /**
   * Is lock set?
   *
   * \retval #Locked      The lock is set.
   * \retval #Not_locked  The lock is not set.
   * \retval #Invalid     The lock does not exist (see valid()).
   */
  Status NO_INSTRUMENT test() const
  {
    auto guard = lock_guard(cpu_lock);
    Address o = _lock_owner.load(cxx::memory_order_relaxed);
    if (EXPECT_FALSE(o & 1))
      return Invalid;
    return o ? Locked : Not_locked;
  }

  /**
   * Acquire the lock with priority inheritance.
   *
   * If the lock is occupied, lend the CPU to current lock owner until we are the
   * lock owner.
   *
   * \retval #Locked      The lock was already locked by the current context.
   * \retval #Not_locked  The current context got the lock (the usual case).
   * \retval #Invalid     The lock does not exist (see valid()).
   */
  Status NO_INSTRUMENT lock()
  {
    auto guard = lock_guard(cpu_lock);

    Mword o = _lock_owner.load(cxx::memory_order_relaxed);
    if (EXPECT_FALSE(o & 1))
      return Invalid;

    Context *c = current();
    if (o == Address(c))
      return Locked;

    do
      {
        for (;;)
          {
            Mword o = _lock_owner.load(cxx::memory_order_relaxed);
            if (o & 1)
              return Invalid;

            if (!o)
              break;

            help(c, reinterpret_cast<Context *>(o), o);
          }
      }
    while (!set_lock_owner(c));
    Mem::mp_wmb();
    c->inc_lock_cnt();   // Do not lose this lock if current is deleted
    return Not_locked;
  }

  Status NO_INSTRUMENT test_and_set()
  {
    return lock();
  }

  void NO_INSTRUMENT clear()
  {
    auto guard = lock_guard(cpu_lock);

    switch_dirty(clear_no_switch_dirty());
  }

  void NO_INSTRUMENT set(Status s)
  {
    if (s == Not_locked)
      clear();
  }

  void NO_INSTRUMENT invalidate()
  {
    _lock_owner.fetch_or((Address)1);
  }

  void NO_INSTRUMENT wait_free()
  {
    auto guard = lock_guard(cpu_lock);
    Context *c = current();

    assert (!valid());

    if (EXPECT_FALSE(lock_owner() == c))
      panic("Current thread owns Switch_lock and attempts to destroy it");

    for(;;)
      {
        assert(cpu_lock.test());

        Address _owner = _lock_owner.load(cxx::memory_order_relaxed);
        Context *owner = reinterpret_cast<Context *>(_owner & ~1UL);
        if (!owner)
          break;

        help(c, owner, _owner);
      }
  }

protected:
  Lock_context NO_INSTRUMENT clear_no_switch_dirty()
  {
    Mem::mp_wmb();
    Lock_context c;
    c.owner = lock_owner();
    clear_lock_owner();
    c.owner->dec_lock_cnt();
    return c;
  }

  /**
   * Do the switch part of clear() after a clear_no_switch_dirty().
   * This function does not touch the lock itself (may be called on
   * an invalid lock).
   *
   * \param c  the context returned by a former clear_no_switch_dirty().
   *
   * \pre must be called atomically with clear_no_switch_dirty(),
   *      under the same cpu lock
   */
  static void NO_INSTRUMENT switch_dirty(Lock_context const &c)
  {
    assert (current() == c.owner);

    Context *curr = c.owner;
    Context *h = curr->helper();

    /*
     * If someone helped us by lending its time slice to us.
     * Just switch back to the helper without changing its helping state.
     */
    bool need_sched = false;

    if (h != curr)
      if (   EXPECT_FALSE(h->home_cpu() != current_cpu())
          || EXPECT_FALSE((long)curr->switch_exec_locked(h, Context::Ignore_Helping)))
        need_sched = true;

    if (!need_sched)
      need_sched = (   curr->lock_cnt() == 0
                    && curr->home_cpu() != current_cpu());

    if (EXPECT_FALSE(need_sched))
      schedule(curr);
  }

private:
  void clear_lock_owner();
  bool set_lock_owner(Context *o);
  static void schedule(Context *curr);

  void NO_INSTRUMENT help(Context *curr, Context *owner, Address owner_id)
  {
    auto s = curr->switch_exec_helping(owner, &_lock_owner, owner_id);
    if (s == Context::Switch::Failed)
      {
        Proc::preemption_point();
        if (curr->home_cpu() != current_cpu())
          curr->schedule();
      }
  }
};

#undef NO_INSTRUMENT
#define NO_INSTRUMENT


#if defined (CONFIG_MP)

inline void NO_INSTRUMENT
Switch_lock::clear_lock_owner()
{
  _lock_owner.fetch_and((Address)1);
}

inline bool NO_INSTRUMENT
Switch_lock::set_lock_owner(Context *o)
{
  bool have_no_locks = o->_lock_cnt.load(cxx::memory_order_relaxed) < 1;

  if (have_no_locks)
    {
      assert (current_cpu() == o->home_cpu());
      while (EXPECT_FALSE(!o->_running_under_lock.try_dispatch()))
        ;
    }
  else
    assert (o->_running_under_lock);

  Mword none = 0;
  if (EXPECT_FALSE(!_lock_owner.compare_exchange_strong(none, Address(o))))
    {
      if (have_no_locks)
        o->_running_under_lock.reset();
      return false;
    }

  return true;
}

inline void
Switch_lock::schedule(Context *curr)
{
  curr->schedule();
  /* we have to recheck the correct setting of `curr->_running_under_lock`
   * after schedule, because the home CPU of `curr` might have been set to
   * the current CPU meanwhile.
   * In this case and if `curr` has no locks anymore _running_under_lock
   * must be `false`. (Invariant: `curr->_running_under_lock` must be true
   * if `curr` executes on any CPU and `lock_cnt` is not zero, or if
   * `lock_cnt` is zero and `curr` is executing on a CPU other than the home
   * CPU of `curr`.)
   */
  if ((curr->home_cpu() == current_cpu())
      && curr->lock_cnt() == 0)
    curr->_running_under_lock.preempt();
}

#else // CONFIG_MP

inline void NO_INSTRUMENT
Switch_lock::clear_lock_owner()
{
  _lock_owner.fetch_and(1, cxx::memory_order_relaxed);
}

/**
 * Set the lock owner.
 *
 * \pre must only be called on a free lock
 */
inline bool NO_INSTRUMENT
Switch_lock::set_lock_owner(Context *o)
{
  _lock_owner.store(Address(o) | (_lock_owner.load(cxx::memory_order_relaxed) & 1), cxx::memory_order_acquire);
  return true;
}

inline void
Switch_lock::schedule(Context *curr)
{ curr->schedule(); }

#endif // CONFIG_MP

