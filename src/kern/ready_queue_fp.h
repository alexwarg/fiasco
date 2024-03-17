#pragma once

#include "config.h"
#include "member_offs.h"
#include "types.h"
#include "globals.h"
#include "cpu_lock.h"
#include "std_macros.h"

#include <cxx/dlist>
#include <cassert>


struct L4_sched_param_fixed_prio : public L4_sched_param
{
  enum : Smword { Class = -1 };
  Mword quantum;
  unsigned short prio;
};

template<typename E>
class Ready_queue_fp
{
  friend class Jdb_thread_list;
  template<typename T>
  friend struct Jdb_thread_list_policy;
  friend class Sched_ctxts_test;
  friend class Scheduler_test;

private:
  typedef typename E::Fp_list List;
  unsigned prio_highest;
  List prio_next[256];

public:
  void set_idle(E *sc)
  { sc->_prio = Config::Kernel_prio; }

  void enqueue(E *i, bool is_current_sched)
  {
    assert(cpu_lock.test());

    // Don't enqueue threads which are already enqueued
    if (EXPECT_FALSE (i->in_ready_list()))
      return;

    unsigned short prio = i->prio();

    if (prio > prio_highest)
      prio_highest = prio;

    prio_next[prio].push(i, is_current_sched ? List::Front : List::Back);
  }

  void dequeue(E *i)
  {
    assert (cpu_lock.test());

    // Don't dequeue threads which aren't enqueued
    if (EXPECT_FALSE (!i->in_ready_list()))
      return;

    unsigned short prio = i->prio();

    prio_next[prio].remove(i);

    while (prio_next[prio_highest].empty() && prio_highest)
      prio_highest--;
  }

  E *next_to_run() const
  { return prio_next[prio_highest].front(); }

  void requeue(E *i)
  {
    if (!i->in_ready_list())
      enqueue(i, false);
    else
      prio_next[i->prio()].rotate_to(*++List::iter(i));
  }


  void deblock_refill(E *)
  {}

};


