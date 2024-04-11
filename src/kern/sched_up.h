#pragma once

#include <context.h>
#include <sched_context.h>
#include <sched.h>
#include <logdefs.h>
#include <context_dbg.h>

template<typename T>
class Sched
{
public:
  using Migration = Context::Migration;

  // Get the ready queue to dequeue from in the case of migartion.
  // The result might be nullptr if the thread is on the invalod CPU
  static Sched_context::Ready_queue *
  migrate_get_ready_queue(Context *c, bool remote)
  {
    if (!remote && c->home_cpu() == current_cpu())
      return &Sched_context::rq.current();
    return nullptr;
  }

  // usually returns a Lock_guard for the pending rqq, does
  // nothing in UP case here.
  [[gnu::warn_unused_result]]
  static nullptr_t lock_and_dequeue_rqq(Context *, bool)
  { return nullptr; }

  static bool
  migrate_to(Context *c, Cpu_number target_cpu, bool)
  {
    if (!Cpu::online(target_cpu))
      {
        c->handle_drq();
        return false;
      }

    bool resched = false;
    if (c->state.has(Thread_ready_mask))
      resched = Sched_context::rq.current()
        .deblock(c->sched(), current()->sched());

    c->enqueue_timeout_again();

    return resched;
  }

  static void
  migrate(Context *c, Migration *info)
  {
    assert (cpu_lock.test());

    LOG_TRACE("Thread migration", "mig", c, Migration_log,
        l->state = c->state();
        l->src_cpu = c->home_cpu();
        l->target_cpu = info->cpu;
        l->user_ip = c->regs()->ip();
    );

    c->_migration = info;
    current()->schedule_if(T::do_migration(c));
  }

  static void
  force_to_invalid_cpu(Context *c)
  {
    // make sure this thread really never runs again by migrating it
    // to the 'invalid' CPU forcefully and then switching to the kernel
    // thread for doing the last bits.
    c->set_home_cpu(Cpu::invalid());
    c->handle_drq();
  }
};
