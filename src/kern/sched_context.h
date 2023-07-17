#pragma once

#include SCHED_CONTEXT_IMPL
#include "globalconfig.h"
#include "per_cpu_data.h"

#include <cassert>

class Sched_context : public Sched_context_t<Sched_context>
{
public:
  class Ready_queue : public Ready_queue_base
  {
  public:
    void set_current_sched(Sched_context *sched);
    void invalidate_sched() { activate(0); }
    /**
     * Deblock the given scheduling context, i.e. add the scheduling context to the
     * ready queue.
     *
     * As an optimization, if requested by setting the `lazy_q` parameter, only adds
     * the deblocked scheduling context to the ready queue if it cannot preempt the
     * currently active scheduling context, i.e. rescheduling is not necessary.
     * Otherwise the caller is responsible to switch to the lazily deblocked
     * scheduling context via `switch_to_locked()`. This is required to ensure that
     * the scheduler does not forget about the scheduling context.
     *
     * \param sc      Sched_context that shall be deblocked.
     * \param crs     Sched_context of the currently running context.
     * \param lazy_q  Queue lazily if applicable.
     *
     * \returns Whether a reschedule is necessary (deblocked scheduling context
     *          can preempt the currently running scheduling context).
     */
    bool deblock(Sched_context *sc, Sched_context *crs, bool lazy_q = false)
    {
      assert(cpu_lock.test());

      Sched_context *cs = current_sched();
      bool res = true;
      if (sc == cs)
        {
          if (crs && crs->dominates(sc))
            res = false;
        }
      else
        {
          deblock_refill(sc);

          if ((EXPECT_TRUE(cs != 0) && cs->dominates(sc))
              || (crs && crs->dominates(sc)))
            res = false;
        }

      if (res && lazy_q)
        return true;

      ready_enqueue(sc);
      return res;
    }

    void ready_enqueue(Sched_context *sc)
    {
      assert(cpu_lock.test());

      // Don't enqueue threads which are already enqueued
      if (EXPECT_FALSE (sc->in_ready_list()))
        return;

      enqueue(sc, true);
    }

    void ready_dequeue(Sched_context *sc)
    {
      assert (cpu_lock.test());

      // Don't dequeue threads which aren't enqueued
      if (EXPECT_FALSE (!sc->in_ready_list()))
        return;

      dequeue(sc);
    }

    void switch_sched(Sched_context *from, Sched_context *to)
    {
      assert (cpu_lock.test());

      // If we're leaving the global timeslice, invalidate it This causes
      // schedule() to select a new timeslice via set_current_sched()
      if (from == current_sched())
        invalidate_sched();

      if (from->in_ready_list())
        dequeue(from);

      enqueue(to, false);
    }

    Context *schedule_in_progress;
  };

  static Per_cpu<Ready_queue> rq;
};

#if defined (CONFIG_JDB)

#include "tb_entry.h"

/** logged scheduling event. */
class Tb_entry_sched : public Tb_entry
{
public:
  unsigned short mode;
  Context const *owner;
  unsigned short id;
  unsigned short prio;
  signed long left;
  unsigned long quantum;

  void print(String_buffer *buf) const;
} __attribute__((packed));

#endif // CONFIG_JDB
