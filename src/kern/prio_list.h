#pragma once

#include <cassert>
#include <cxx/dlist>
#include <cxx/hlist>
#include "member_offs.h"
#include "spin_lock.h"
#include <lock_guard.h>
#include "types.h"

/**
 * Priority sorted list with insert complexity O(n) n = number of available
 * priorities (256 in Fiasco).
 */


class Prio_list;

/**
 * Single element of a priority sorted list.
 */
class Prio_list_elem : public cxx::H_list_item, public cxx::D_list_item
{
  MEMBER_OFFSET();

public:
  unsigned short prio() const
  { return _prio; }

  /**
   * Is the element actually enqueued?
   * @return true if the element is actaully enqueued in a list.
   */
  bool in_list() const { return S_list::in_list(this); }

  /** Current receiver.
      @return receiver this sender is currently trying to send a message to.
   */
  Prio_list *wait_queue() const
  {
    return _queue;
  }

private:
  friend class Prio_list;
  friend class Jdb_sender_list;
  typedef cxx::D_list_cyclic<Prio_list_elem> S_list;
  cxx::atomic<Prio_list *> _queue = nullptr;

  /**
   * Priority, the higher the better.
   */
  unsigned short _prio;

  /**
   * Setup pointers for enqueue.
   */
  void init(unsigned short p)
  {
    _prio = p;
  }

};


/**
 * Priority sorted list.
 *
 * The list is organized in a way that the highest priority member can be
 * found with O(1). Every dequeue operation is also O(1).
 *
 * There is a forward-iterable list with at most one element per priority.
 * Elements with the same priority are handled in a double-linked circular
 * list for each priority. This double-linked list implements FIFO policy for
 * finding the next element.
 */
class Prio_list : private cxx::H_list<Prio_list_elem>
{
  MEMBER_OFFSET();
  friend class Jdb_sender_list;
  friend class Prio_list_tester;

public:
  typedef cxx::H_list<Prio_list_elem> P_list;
  typedef cxx::D_list_cyclic<Prio_list_elem> S_list;

  using P_list::front;
  using P_list::empty;

  class Pq_lock
  {
    cxx::atomic<char> _l{0};
  public:
    enum Status
    {
      Not_locked = 0,
      Locked = 2,
      Invalid = 4,
    };

    Status test_and_set() noexcept
    {
      char old = _l.load(cxx::memory_order_acquire);
      for (;;)
        {
          if (old & 4)
            return Invalid;

          if (old & 2)
            {
              Proc::pause();
              old = _l.load(cxx::memory_order_acquire);
              continue;
            }

          if (_l.compare_exchange_weak(old, old | 2, cxx::memory_order_acquire))
            return Not_locked;
        }
    }

    Status test() noexcept
    {
      return Status(_l.load() & char(Locked));
    }

    void set(Status) noexcept
    {
      _l.fetch_and(static_cast<char>(~2));
    }

    void invalidate() noexcept
    {
      _l.fetch_or(4);
    }

    bool invalid() const noexcept
    {
      return _l.load() & 4;
    }
  };

  Pq_lock *qlock() { return &_lock; }

  Prio_list_elem *first() const { return front(); }

  Prio_list_elem *next(Prio_list_elem *e) const
  {
    S_list::Iterator i = ++S_list::iter(e);
    if (P_list::in_list(*i))
      return *++P_list::iter(*i);
    return *i;
  }

  /**
   * Insert a new element into the priority list.
   * @param e the element to insert
   * @param prio the priority for the element
   */
  bool insert_dirty(Prio_list_elem *e, unsigned short prio) __attribute__((nonnull))
  {
    Prio_list *old = nullptr;
    if (!e->_queue.compare_exchange_strong(old, this, cxx::memory_order_acquire))
      return false;

    e->init(prio);

    Iterator pos = begin();

    while (pos != end() && pos->prio() > prio)
      ++pos;

    if (pos != end() && pos->prio() == prio)
      S_list::insert_before(e, S_list::iter(*pos));
    else
      {
        S_list::self_insert(e);
        insert_before(e, pos);
      }

    return true;
  }

  bool insert(Prio_list_elem *e, unsigned short prio) __attribute__((nonnull))
  {
    auto guard = lock_guard(_lock);
    return insert_dirty(e, prio);
  }



  /**
   * Dequeue a given element from the list.
   * @param e the element to dequeue
   */
  bool dequeue(Prio_list_elem *e)
  {
    if (e->_queue.load() != this)
      return false;

    auto guard = lock_guard(_lock);
    Prio_list *old = this;
    if (!e->_queue.compare_exchange_strong(old, nullptr, cxx::memory_order_release))
      return false;

    Prio_list_elem **c = nullptr;
    if (EXPECT_FALSE(_cursor != nullptr) && EXPECT_FALSE(_cursor == e))
      c = &_cursor;

    _dequeue(e, c);
    return true;
  }

  Prio_list_elem *dequeue_first_dirty()
  {
    Prio_list_elem *f = first();
    if (!f)
      return nullptr;

    Prio_list *old = this;
    if (!f->_queue.compare_exchange_strong(old, nullptr, cxx::memory_order_release))
      return nullptr;

    Prio_list_elem **c = nullptr;
    if (EXPECT_FALSE(_cursor != nullptr) && EXPECT_FALSE(_cursor == f))
      c = &_cursor;

    _dequeue(f, c);
    return f;
  }

  Prio_list_elem *dequeue_first()
  {
    auto guard = lock_guard(_lock);
    return dequeue_first_dirty();
  }

  void cursor(Prio_list_elem *e)
  { _cursor = e; }

  Prio_list_elem *cursor() const
  { return _cursor; }

private:
  Prio_list_elem *_cursor = nullptr;
  //Spin_lock<> _lock{Spin_lock<>::Unlocked};
  Pq_lock _lock;

  /**
   * Dequeue a given element from the list.
   * @param e the element to dequeue
   */
  void _dequeue(Prio_list_elem *e, Prio_list_elem **next)
  {
    if (P_list::in_list(e))
      {
        assert (S_list::in_list(e));
        // yes we are the head of our priority
        if (S_list::has_sibling(e))
          {
            P_list::replace(e, *++S_list::iter(e));
            if (next) *next = *++S_list::iter(e);
          }
        else
          {
            if (next) *next = *++P_list::iter(e);
            P_list::remove(e);
          }
      }
    else
      {
        if (next)
          *next = Prio_list::next(e);
      }
    S_list::remove(e);
  }
};

using Iterable_prio_list = Prio_list;
using Locked_prio_list = Iterable_prio_list;

