#pragma once

#include <cxx/hlist>
#include <climits>
#include <cassert>

#include <l4_types.h>
#include <cpu_lock.h>
#include <lock_guard.h>
#include <per_cpu_data.h>
#include <timer.h>
#include <config.h>
#include <system_clock.h>

/** A timeout basic object. It contains the necessary queues and handles
    enqueuing, dequeuing and handling of timeouts. Real timeout classes
    should overwrite expired(), which will do the real work, if an
    timeout hits.
 */
class Timeout : public cxx::H_list_item
{
  friend class Jdb_timeout_list;
  friend class Jdb_list_timeouts;
  friend class Timeout_q;
  friend class Timeouts_test;

public:
  typedef cxx::H_list<Timeout> To_list;

  Timeout(Timeout &&) = delete;
  Timeout(const Timeout&) = delete;
  virtual ~Timeout() noexcept = default;

  Timeout() noexcept : _flags({0, 0})
  {}

  /**
   * Initializes an timeout object.
   */
  void init()
  {
    _wakeup = ULLONG_MAX;
  }

  /**
   * Check if timeout is set.
   */
  bool is_set() const noexcept
  {
    return To_list::in_list(this);
  }

  /**
   * Check if timeout has hit.
   */
  bool has_hit() const noexcept
  {
    return _flags.hit;
  }


  /**
   * Return remaining time of timeout.
   */
  Signed64 get_timeout(Unsigned64 clock) const noexcept
  {
    return _wakeup - clock;
  }

  void set(Unsigned64 clock, Cpu_number cpu);
  void set_again(Cpu_number cpu);

  /**
   * Reset timeout, preventing its expiration.
   *
   * \pre `cpu_lock` must be held
   */
  void reset()
  {
    assert (cpu_lock.test());
    To_list::remove(this);

    // Normally we should reprogram the timer in one shot mode
    // But we let the timer interrupt handler to do this "lazily", to save cycles
  }

  /**
   * Dequeue an expired timeout.
   * @return true if a reschedule is necessary, false otherwise.
   */
  bool expire()
  {
    _flags.hit = 1;
    return expired();
  }

protected:
  /**
   * Absolute system time we want to be woken up at.
   */
  Unsigned64 _wakeup;

private:
  /**
   * Overwritten timeout handler function.
   * @return true if a reschedule is necessary, false otherwise.
   */
  virtual bool expired();

  struct
  {
    bool     hit  : 1;
    unsigned res  : 6; // performance optimization
  } _flags;
};


class Timeout_q
{
  friend class Timeouts_test;
private:
  /**
   * Timeout queue count (2^n) and  distance between two queues in 2^n.
   */
  enum
  {
    Wakeup_queue_count	  = 8,
    Wakeup_queue_distance = 12 // i.e. (1<<12)us
  };

  typedef Timeout::To_list To_list;
  typedef To_list::Iterator Iterator;
  typedef To_list::Const_iterator Const_iterator;

  /**
   * The timeout queues.
   */
  To_list _q[Wakeup_queue_count];

  /**
   * The current programmed timeout.
   */
  Unsigned64 _current;
  Unsigned64 _old_clock;

  /**
   * Program the timer interrupt to the given timeout.
   *
   * Enforces a lower bound for the timeout, as defined by
   * Config::One_shot_min_interval_us, to prevent too frequent handling of the
   * timeout queue.
   */
  void program_timer(Unsigned64 timeout)
  {
    if (timeout < _old_clock + Config::One_shot_min_interval_us)
      _current = _old_clock + Config::One_shot_min_interval_us;
    else
      _current = timeout;

    Timer::update_timer(_current);
  }

  /**
   * Update the timer interrupt to the next timeout.
   *
   * Set the timer to the next timeout in one-shot mode. The parameter
   * gives a hint for the maximum timeout to set.
   *
   * \param max_timeout  Maximum timeout to set.
   *                     Use Timer::Infinite_timeout for no timeout.
   */
  void update_timer(Unsigned64 max_timeout)
  {
    if constexpr (!Config::Scheduler_one_shot)
      return;

    Unsigned64 next_timeout = max_timeout;

    // scan all queues for the next minimum
    for (int i = 0; i < Wakeup_queue_count; i++)
      {
        // make sure that something enqueued other than the dummy element
        if (first(i).empty())
          continue;

        if (first(i).front()->_wakeup < next_timeout)
          next_timeout = first(i).front()->_wakeup;
      }

    program_timer(next_timeout);
  }

public:
  static Per_cpu<Timeout_q> timeout_queue;

  To_list &first(int index)
  { return _q[index & (Wakeup_queue_count-1)]; }

  To_list const &first(int index) const
  { return _q[index & (Wakeup_queue_count-1)]; }

  unsigned queues() const
  { return Wakeup_queue_count; }

  /**
   * Enqueue a new timeout.
   */
  void enqueue(Timeout *to)
  {
    int queue = (to->_wakeup >> Wakeup_queue_distance) & (Wakeup_queue_count-1);

    To_list &q = first(queue);
    Iterator tmp = q.begin();

    while (tmp != q.end() && tmp->_wakeup < to->_wakeup)
      ++tmp;

    q.insert_before(to, tmp);

    if constexpr (! Config::Scheduler_one_shot)
      return;

    if (to->_wakeup < _current)
      program_timer(to->_wakeup);
  }

  /**
   * Handles the timeouts by calling expired() for the expired timeouts and
   * programming the "oneshot timer" to the next timeout.
   *
   * \param now  The system clock used to decide if timeouts have expired.
   *
   * \retval true if a reschedule is necessary.
   * \retval false otherwise.
   */
  bool do_timeouts(Unsigned64 now)
  {
    bool reschedule = false;

    // We test if the time between 2 activations of this function is greater
    // than the distance between two timeout queues.
    // To avoid losing timeouts, we calculate the missed queues and scan them
    // too.
    // This can only happen, if we don't enter the timer interrupt for a long
    // time, usual with one-shot timer.
    // Because we initialize old_dequeue_time with zero,
    // we can get a "miss" on the first timer interrupt.
    // But this is booting the system, which is uncritical.

    // Calculate which timeout queues needs to be checked.
    int start = (_old_clock >> Wakeup_queue_distance);
    int diff  = (now >> Wakeup_queue_distance) - start;
    int end   = (start + diff + 1) & (Wakeup_queue_count - 1);

    // wrap around
    start = start & (Wakeup_queue_count - 1);

    // test if an complete miss
    if (diff >= Wakeup_queue_count)
      start = end = 0; // scan all queues

    // update old_clock for the next run
    _old_clock = now;

    // ensure we always terminate
    assert((end >= 0) && (end < Wakeup_queue_count));

    for (;;)
      {
        To_list &q = first(start);
        Iterator timeout = q.begin();

        // now scan this queue for timeouts below current clock
        while (timeout != q.end() && timeout->_wakeup <= now)
          {
            Timeout *to = *timeout;
            timeout = q.erase(timeout);
            reschedule |= to->expire();
          }

        // next queue
        start = (start + 1) & (Wakeup_queue_count - 1);

        if (start == end)
          break;
      }

    update_timer(now + Config::One_shot_max_interval_us);
    return reschedule;
  }

  Timeout_q() noexcept
  : _current(ULLONG_MAX), _old_clock(0)
  {}

  bool have_timeouts(Timeout const *ignore) const
  {
    for (unsigned i = 0; i < Wakeup_queue_count; ++i)
      {
        To_list const &t = first(i);
        if (!t.empty())
          {
            To_list::Const_iterator f = t.begin();
            if (*f == ignore && (++f) == t.end())
              continue;

            return true;
          }
      }

    return false;
  }

};

/**
 * Program timeout to expire at the specified wakeup time.
 *
 * \param clock  Wakeup time
 * \param cpu    CPU on which the timeout shall be queued
 * \pre `cpu` == current CPU
 * \pre Timeout must not be set
 */
inline void
Timeout::set(Unsigned64 clock, Cpu_number cpu)
{
  // XXX uses global kernel lock
  auto guard = lock_guard(cpu_lock);
  assert(cpu == current_cpu());

  assert (!is_set());

  _wakeup = clock;
  Timeout_q::timeout_queue.cpu(cpu).enqueue(this);
}

/**
 * Program reset timeout to expire at the originally set wakeup time, unless
 * it already has been hit.
 *
 * \param cpu  CPU on which the timeout shall be queued
 * \pre `cpu` == current CPU
 * \pre Timeout must not be set
 */
inline void
Timeout::set_again(Cpu_number cpu)
{
  // XXX uses global kernel lock
  auto guard = lock_guard(cpu_lock);
  assert(cpu == current_cpu());

  assert(! is_set());
  if (has_hit())
    return;

  Timeout_q::timeout_queue.cpu(cpu).enqueue(this);
}
