
#include <context.h>

#include <cassert>
#include <cxx/atomic>

#include <cpu.h>
#include <cpu_lock.h>
#include <entry_frame.h>
#include <globals.h>		// current()
#include <lock_guard.h>
#include <logdefs.h>
#include <mem.h>
#include <mem_layout.h>
#include <processor.h>
#include <space.h>
#include <std_macros.h>
#include <thread_state.h>
#include <timer.h>
#include <timeout.h>
#include <cassert>

DEFINE_PER_CPU Per_cpu<Clock> Context::_clock(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU Per_cpu<Context *> Context::_kernel_ctxt;

[[gnu::flatten]]
void
Context::switchin_context(Context *from)
{
  assert (this == current());
  assert (state() & Thread_ready_mask);
  switchin_context_arch(from);
}

void
Context::schedule()
{
  auto guard = lock_guard(cpu_lock);
  assert (!Sched_context::rq.current().schedule_in_progress);

  // we give up the CPU as a helpee, so we have no helper anymore
  if (EXPECT_FALSE(helper() != this))
    set_helper(Not_Helping);

  // if we are a thread on a foreign CPU we must ask the kernel context to
  // schedule for us
  Cpu_number current_cpu = ::current_cpu();
  while (EXPECT_FALSE(current_cpu != access_once(&_home_cpu)))
    {
      Context *kc = Context::kernel_context(current_cpu);
      assert (this != kc);

      // flag that we need to schedule
      kc->state.add_dirty(Thread_need_resched);
      switch (switch_exec_locked(kc, Ignore_Helping))
        {
        case Switch::Ok:
          return;
        case Switch::Resched:
          current_cpu = ::current_cpu();
          continue;
        case Switch::Failed:
          assert (false);
          continue;
        }
    }

  // now, we are sure that a thread on its home CPU calls schedule.
  CNT_SCHEDULE;

  // Ensure only the current thread calls schedule
  assert (this == current());

  Sched_context::Ready_queue *rq = &Sched_context::rq.current();

  // Enqueue current thread into ready-list to schedule correctly
  update_ready_list();

  // Select a thread for scheduling.
  Context *next_to_run;

  for (;;)
    {
      next_to_run = rq->next_to_run()->context();

      // Ensure ready-list sanity
      assert (next_to_run);

      if (EXPECT_FALSE(!next_to_run->state.has(Thread_ready_mask)))
        rq->ready_dequeue(next_to_run->sched());
      else switch (schedule_switch_to_locked(next_to_run))
        {
        default:
        case Switch::Ok:      return;   // ok worked well
        case Switch::Failed:  break;    // not migrated, need preemption point
        case Switch::Resched:
          {
            Cpu_number n = ::current_cpu();
            if (n != current_cpu)
              {
                current_cpu = n;
                rq = &Sched_context::rq.current();
              }
          }
          continue; // may have been migrated...
        }

      rq->schedule_in_progress = this;
      Proc::preemption_point();
      if (EXPECT_TRUE(current_cpu == ::current_cpu()))
        rq->schedule_in_progress = 0;
      else
        return; // we got migrated and selected on our new CPU, so we may run
    }
}

void
Context::activate()
{
  auto guard = lock_guard(cpu_lock);
  if (xcpu_state_change(~0UL, Thread_ready, true))
    current()->switch_to_locked(this);
}

// queue operations

// XXX for now, synchronize with global kernel lock
//-
Context::Switch
Context::switch_exec_locked(Context *t, enum Helping_mode mode)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // Time-slice lending: if t is locked, switch to its locker
  // instead, this is transitive
  //

  if (EXPECT_FALSE(t->running_on_different_cpu()))
    {
      if (!t->in_ready_list())
        Sched_context::rq.current().ready_enqueue(t->sched());
      return Switch::Failed;
    }


  LOG_CONTEXT_SWITCH;
  CNT_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!t->state.has(Thread_ready_mask)))
    {
      assert (state.has(Thread_ready_mask));
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_cpu_state.kernel_sp);

  t->set_helper(mode);

  if (EXPECT_TRUE(get_current_cpu() == home_cpu()))
    update_ready_list();

  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);

  return switch_handle_drq();
}

Context::Switch
Context::switch_exec_helping(Context *t, cxx::atomic<Mword> const *lock, Mword val)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // we actually hold locks
  if (!t->need_help(lock, val))
    return Switch::Failed;

  LOG_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!t->state.has(Thread_ready_mask)))
    {
      assert (state.has(Thread_ready_mask));
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_cpu_state.kernel_sp);

  if (EXPECT_TRUE(get_current_cpu() == home_cpu()))
    update_ready_list();

  t->set_helper(Helping);
  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);
  return switch_handle_drq();
}

bool
Context::rcu_unblock(Rcu_item *i)
{
  assert(cpu_lock.test());
  return static_cast<Context*>(i)->xcpu_state_change(~Thread_waiting, Thread_ready);
}

