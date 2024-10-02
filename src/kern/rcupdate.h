#pragma once

#include "cpu_mask.h"
#include "cpu_lock.h"
#include "per_cpu_data.h"
#include "spin_lock.h"
#include "lock_guard.h"
#include "logdefs.h"
#include <cxx/slist>

#if defined(CONFIG_JDB)
#include "logdefs.h"
#include "string_buffer.h"
#endif

class Rcu_data;

/**
 * \brief Encapsulation of RCU batch number.
 */
class Rcu_batch
{
  friend class Jdb_rcupdate;
public:
  /// create uninitialized batch.
  Rcu_batch() = default;
  /// create a batch initialized with \a b.
  constexpr Rcu_batch(long b) noexcept : _b(b) {}

  /// less than comparison.
  bool operator < (Rcu_batch const &o) const noexcept { return (_b - o._b) < 0; }
  /// greater than comparison.
  bool operator > (Rcu_batch const &o) const noexcept { return (_b - o._b) > 0; }
  /// greater than / equal to comparison.
  bool operator >= (Rcu_batch const &o) const noexcept { return (_b - o._b) >= 0; }
  /// equality check.
  bool operator == (Rcu_batch const &o) const noexcept { return _b == o._b; }
  /// inequality test.
  bool operator != (Rcu_batch const &o) const noexcept { return _b != o._b; }
  /// increment batch.
  Rcu_batch &operator ++ () noexcept { ++_b; return *this; }
  /// increase batch with \a r.
  Rcu_batch operator + (long r) noexcept { return Rcu_batch(_b + r); }


private:
  long _b;
};

/**
 * \brief Item that can bequeued for the next grace period.
 *
 * An RCU item is basically a pointer to a callback which is called
 * after one grace period.
 */
class Rcu_item : public cxx::S_list_item
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;

private:
  bool (*_call_back)(Rcu_item *);
};


/**
 * \brief List of Rcu_items.
 *
 * RCU lists are used a lot of times in the RCU implementation and are
 * implemented as single linked lists with FIFO semantics.
 *
 * \note Concurrent access to the list is not synchronized.
 */
class Rcu_list : public cxx::S_list_tail<Rcu_item>
{
private:
  typedef cxx::S_list_tail<Rcu_item> Base;
public:
  Rcu_list() = default;
  Rcu_list(Rcu_list &&o) noexcept : Base(static_cast<Base &&>(o)) {}
  Rcu_list &operator = (Rcu_list &&o) noexcept
  {
    Base::operator = (static_cast<Base &&>(o));
    return *this;
  }

  /**
   * \brief Enqueue Rcu_item into the list (at the tail).
   * \prarm i the RCU item to enqueue.
   */
  void enqueue(Rcu_item *i) noexcept { push_back(i); }


private:
  friend class Jdb_rcupdate;
};

/**
 * \brief Global RCU data structure.
 */
class Rcu_glbl
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;

private:
  Rcu_batch _current;      ///< current batch
  Rcu_batch _completed;    ///< last completed batch
  bool _next_pending;      ///< Are there items in batch `_current + 1`?
  Spin_lock<> _lock;
  Cpu_mask _cpus;          ///< CPUs waiting for a quiescent state

  Cpu_mask _active_cpus;   ///< CPUs participating in RCU

public:
  Rcu_glbl()
  : _current(-300), _completed(-300)
  {}

private:
  /**
   * Start a new grace period if there are callbacks queued for the next batch and
   * the current batch is completed.
   *
   * \pre #_lock must be locked
   */
  void start_batch() noexcept
  {
    if (_next_pending && _completed == _current)
      {
        _next_pending = false;
        Mem::mp_wmb();
        ++_current;
        Mem::mp_mb();
        _cpus = _active_cpus;
      }
  }

  /**
   * Announce a quiescent state of a CPU.
   *
   * If no CPU is left waiting for a quiescent state, the current grace period is
   * finished and the current batch is completed. If additionally there are
   * callbacks queued for the next batch (#_next_pending), a new grace period is
   * started (cf. start_batch()).
   *
   * \pre #_lock must be locked
   */
  void cpu_quiet(Cpu_number cpu) noexcept
  {
    _cpus.clear(cpu);
    if (_cpus.empty())
      {
        _completed = _current;
        start_batch();
      }
  }
};

/**
 * \brief CPU local data structure for RCU.
 */
class Rcu_data
{
  friend class Jdb_rcupdate;
public:

  Rcu_batch _q_batch;   ///< batch no. for grace period
  bool _q_passed;       ///< quiescent state for batch `_q_batch` passed?
  bool _pending;        ///< waiting for quiescent state for batch `_q_batch`?
  bool _idle;           ///< `false` iff CPU `_cpu` is participating in RCU

  Rcu_batch _batch;     ///< batch no. assigned to the items in `_c`
  Rcu_list _n;          ///< new items: waiting for being assigned to a batch
  long _len;            ///< number of items in `_n + _c + _d` (for debugging)
  Rcu_list _c;          ///< current items: assigned to batch `_batch`
  Rcu_list _d;          ///< done items: waited a full grace period
  Cpu_number _cpu;      ///< the CPU this `Rcu_data` instance is assigned to

public:
  Rcu_data(Cpu_number cpu)
  : _idle(true), _cpu(cpu)
  {}

  ~Rcu_data();

  [[nodiscard]] bool process_callbacks(Rcu_glbl *rgp);

  /**
   * \pre must run under cpu lock
   */
  void enqueue(Rcu_item *i) noexcept
  {
    _n.enqueue(i);
    ++_len;
  }

  void enter_idle(Rcu_glbl *rgp) noexcept
  {
    if (EXPECT_TRUE(!_idle))
      {
        _idle = true;

        auto guard = lock_guard(rgp->_lock);
        rgp->_active_cpus.clear(_cpu);

        if (_q_batch != rgp->_current || _pending)
          {
            _q_batch = rgp->_current;
            _pending = false;
            rgp->cpu_quiet(_cpu);
            assert (!pending(rgp));
          }
      }
  }

  bool pending(Rcu_glbl *rgp) const noexcept
  {
    // The CPU has pending RCU callbacks and the grace period for them
    // has been completed.
    if (!_c.empty() && rgp->_completed >= _batch)
      return 1;

    // The CPU has no pending RCU callbacks, however there are new callbacks
    if (_c.empty() && !_n.empty())
      return 1;

    // The CPU has callbacks to be invoked finally
    if (!_d.empty())
      return 1;

    // RCU waits for a quiescent state from the CPU
    if ((_q_batch != rgp->_current) || _pending)
      return 1;

    // OK, no RCU work to do
    return 0;
  }

private:
  bool do_batch() noexcept
  {
    int count = 0;
    bool need_resched = false;
    for (Rcu_list::Const_iterator l = _d.begin(); l != _d.end();)
      {
        Rcu_item *i = *l;
        ++l;

        need_resched |= i->_call_back(i);
        ++count;
      }

    // XXX: I do not know why this and the former stuff is w/o cpu lock
    //      but the couting needs it?
    _d.clear();

    // XXX: we use clear, we seemingly worked through the whole list
    //_d.head(l);

      {
        auto guard = lock_guard(cpu_lock);
        _len -= count;
      }

    return need_resched;
  }

  void check_quiescent_state(Rcu_glbl *rgp)
  {
    if (_q_batch != rgp->_current)
      {
        // start new grace period
        _pending = true;
        _q_passed = false;
        _q_batch = rgp->_current;
        return;
      }

    // Is the grace period already completed for this cpu?
    // use _pending, not bitmap to avoid cache trashing
    if (!_pending)
      return;

    // Was there a quiescent state since the beginning of the grace period?
    if (!_q_passed)
      return;

    _pending = false;

    auto guard = lock_guard(rgp->_lock);

    if (EXPECT_TRUE(_q_batch == rgp->_current))
      rgp->cpu_quiet(_cpu);
  }

  void move_batch(Rcu_list &l) noexcept
  {
    auto guard = lock_guard(cpu_lock);
    _n.append(l);
  }
};



/**
 * \brief Encapsulation of RCU implementation.
 *
 * This class aggregates per CPU data structures as well as the global
 * data structure for RCU and provides a common RCU interface.
 */
class Rcu
{
  friend class Rcu_data;
  friend class Jdb_rcupdate;
  friend class Rcu_tester;

public:
  /// The lock to prevent a quiescent state.
  typedef Cpu_lock Lock;
  static Rcu_glbl *rcu() { return &_rcu; }

  static void enter_idle(Cpu_number cpu) noexcept
  {
    Rcu_data *rdp = &_rcu_data.cpu(cpu);
    if (EXPECT_TRUE(!rdp->_idle))
      {
        LOG_TRACE("Rcu idle", "rcu", ::current(), Log_rcu,
            l->cpu = cpu;
            l->item = 0;
            l->event = Rcu_idle);
      }

    rdp->enter_idle(rcu());
  }

  static void leave_idle(Cpu_number cpu) noexcept
  {
    Rcu_data *rdp = &_rcu_data.cpu(cpu);
    if (EXPECT_FALSE(rdp->_idle))
      {
        LOG_TRACE("Rcu idle", "rcu", ::current(), Log_rcu,
            l->cpu = cpu;
            l->item = 0;
            l->event = Rcu_unidle);

        rdp->_idle = false;
        auto guard = lock_guard(rcu()->_lock);
        rcu()->_active_cpus.set(cpu);
        rdp->_q_batch = Rcu::rcu()->_current;
      }
  }

  static void call(Rcu_item *i, bool (*cb)(Rcu_item *)) noexcept
  {
    i->_call_back = cb;
    LOG_TRACE("Rcu call", "rcu", ::current(), Log_rcu,
        l->cpu   = current_cpu();
        l->event = Rcu_call;
        l->item = i;
        l->cb = (void*)cb);

    auto guard = lock_guard(cpu_lock);

    Rcu_data *rdp = &_rcu_data.current();
    rdp->enqueue(i);
  }

  [[nodiscard]] static bool
  process_callbacks() noexcept
  { return _rcu_data.current().process_callbacks(&_rcu); }

  [[nodiscard]] static bool
  process_callbacks(Cpu_number cpu) noexcept
  { return _rcu_data.cpu(cpu).process_callbacks(&_rcu); }

  static bool
  pending(Cpu_number cpu) noexcept
  {
    return _rcu_data.cpu(cpu).pending(&_rcu);
  }

  static bool
  idle(Cpu_number cpu) noexcept
  {
    Rcu_data const *d = &_rcu_data.cpu(cpu);
    return d->_c.empty() && !d->pending(&_rcu);
  }

  static void
  inc_q_cnt(Cpu_number cpu) noexcept
  { _rcu_data.cpu(cpu)._q_passed = true; }

  static void
  schedule_callbacks(Cpu_number cpu, Unsigned64 clock);

  static Rcu::Lock *
  lock() noexcept
  { return &cpu_lock; }

  static bool
  do_pending_work(Cpu_number cpu) noexcept
  {
    if (pending(cpu))
      {
        inc_q_cnt(cpu);
        return process_callbacks(cpu);
      }
    return false;
  }

private:
  static Rcu_glbl _rcu;
  static Per_cpu<Rcu_data> _rcu_data;

#if defined(CONFIG_JDB)
public:
  struct Log_rcu : public Tb_entry
  {
    Cpu_number cpu;
    Rcu_item *item;
    void *cb;
    unsigned char event;
    void print(String_buffer *buf) const
    {
      char const *events[] = { "call", "process", "idle", "unidle" };
      buf->printf("rcu-%s (cpu=%u) item=%p", events[event],
                  cxx::int_value<Cpu_number>(cpu), item);
    }
  };

  enum Rcu_events
  {
    Rcu_call = 0,
    Rcu_process = 1,
    Rcu_idle = 2,
    Rcu_unidle = 3,
  };
#endif
};

