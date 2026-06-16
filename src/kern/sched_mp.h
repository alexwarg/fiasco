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
  static Sched_context::Ready_queue *
  migrate_get_ready_queue(Context *c, bool remote)
  {
    return EXPECT_TRUE(!remote)
           ? &Sched_context::rq.current()
           : &Sched_context::rq.cpu(c->home_cpu());
  }

  // return remove us from the pending rqq with grabbing the q lock and
  // returning a Lock_guard if we actually locked the queue.
  [[nodiscard]]
  static decltype(lock_guard(Context::_pending_rqq.current().q_lock()))
  lock_and_dequeue_rqq(Context *c, bool do_lock)
  {
    Queue &q = Context::_pending_rqq.current();
    // The queue lock of the current CPU protects the cpu number in
    // the thread

    auto g = do_lock ? lock_guard(q.q_lock()) : lock_guard_dont_lock(q.q_lock());

    assert (q.q_lock()->test());
    // potentially dequeue from our local queue
    if (c->_pending_rq.queued())
      check (q.dequeue(&c->_pending_rq));

    return g;
  }

  static bool
  migrate_to(Context *c, Cpu_number target_cpu, bool /*remote*/)
  {
    bool ipi = false;
      {
        Queue &q = c->_pending_rqq.cpu(target_cpu);
        auto g = lock_guard(q.q_lock());

        if (c->atomic_home_cpu() == target_cpu
            && EXPECT_FALSE(!Cpu::online(target_cpu)))
          {
            c->handle_drq();
            return false;
          }

        // migrated meanwhile
        if (c->atomic_home_cpu() != target_cpu
            || c->_pending_rq.queued())
          return false;

        if (target_cpu == current_cpu())
          {
            g.reset();
            bool resched = c->handle_drq();
            return resched | Sched_context::rq.current()
              .deblock(c->sched(), current()->sched());
          }

        ipi = c->pending_rqq_do_enqueue(&q);
      }

    if (ipi)
      {
        //LOG_MSG_3VAL(this, "sipi", current_cpu(), cpu(), (Mword)current());
        Ipi::send(Ipi::Request, current_cpu(), target_cpu);
      }

    return false;
  }

  static bool
  migrate_xcpu(Context *c, Cpu_number cpu)
  {
    bool ipi = false;

      {
        Queue &q = Context::_pending_rqq.cpu(cpu);
        auto g = lock_guard(q.q_lock());

        // already migrated
        if (cpu != c->atomic_home_cpu())
          return false;

        // now we are sure that this thread stays on 'cpu' because
        // we have the rqq lock of 'cpu'
        if (!Cpu::online(cpu))
          {
            auto inf = T::start_migration(c);

            if (inf.is_done())
              return inf.resched(); // all done, nothing to do

            Cpu_number target_cpu = access_once(&inf.get()->cpu);
            T::migrate_away(c, inf.get(), true);
            g.reset();
            return migrate_to(c, target_cpu, true);
            // FIXME: Wie lange dauert es ready dequeue mit WFQ zu machen?
            // wird unter spinlock gemacht !!!!
          }

        ipi = c->pending_rqq_do_enqueue(&q);
      }

    if (ipi)
      Ipi::send(Ipi::Request, current_cpu(), cpu);

    return false;
  }

  static void
  migrate(Context *c, Context::Migration *info)
  {
    assert (cpu_lock.test());

    LOG_TRACE("Thread migration", "mig", c, Migration_log,
        l->state = c->state();
        l->src_cpu = c->home_cpu();
        l->target_cpu = info->cpu;
        l->user_ip = c->regs()->ip();
    );
      {
        Migration *old = c->_migration.exchange(info);

        // flag old migration to be done / stale
        if (old)
          old->set_done();
      }

    Cpu_number cpu = c->home_cpu();

    if (current_cpu() == cpu)
      current()->schedule_if(T::do_migration(c));
    else
      current()->schedule_if(migrate_xcpu(c, cpu));

    info->wait_locked();
  }
  static void
  try_finish_migration(Context *c)
  {
    if (c->state.change_safely(~Thread_finish_migration, 0))
      c->finish_migration();
  }

  template<typename CONTEXT>
  static bool
  handle_remote_request(CONTEXT *self, Context **mq, Context *curr)
  {
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
            resched |= T::initiate_migration(self);

            // if migrated away skip the resched test below
            if (self->atomic_home_cpu() != curr->get_current_cpu())
              return resched;
          }
        else
          *mq = self;
      }
    else
      try_finish_migration(self);

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

  static bool
  take_cpu_offline(Cpu_number cpu, bool drain_rqq = false)
  {
    assert (cpu == current_cpu());
    assert (!Proc::interrupts());

    for (;;)
      {
        auto &q = Context::_pending_rqq.current();

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
        q.handle_requests<T>(current(), &migration_q);
        // assume we run from the idle thread, and the idle thread does
        // never migrate so `migration_q` must be 0
        assert (!migration_q);
      }

    Mem::mp_mb();

    // As the interrupts are disabled (this is acceptable as this function is
    // called during system suspend only), the loop safely drains all the RCU
    // queues of the current CPU without race conditions. And the enter_idle()
    // does safely remove the CPU from the list of active CPUs.
    while (!Rcu::idle(cpu))
      {
        Rcu::do_pending_work(cpu);
        Proc::pause();
      }
    Rcu::enter_idle(cpu);

    Cpu_call::handle_global_requests();

    return true;
  }

  static void
  handle_remote_requests_irq()
  {
    assert (cpu_lock.test());
    // printf("CPU[%2u]: > RQ IPI (current=%p)\n", current_cpu(), current());
    Context *const c = current();
    Ipi::eoi(Ipi::Request, current_cpu());
    //LOG_MSG_3VAL(c, "ipi", cxx::int_value<Cpu_number>(c->home_cpu()), (Mword)c, c->drq_pending());

    // we might have to migrate the currently running thread, and we cannot do
    // this during the processing of the request queue. In this case we get the
    // thread in migration_q and do this here.
    Context *migration_q = 0;
    bool resched = Context::_pending_rqq.current()
      .handle_requests<T>(c, &migration_q);

    resched |= Rcu::do_pending_work(current_cpu());

    if (migration_q)
      resched |= T::do_migration(migration_q);

    bool on_current_cpu = c->home_cpu() == current_cpu();
    if (on_current_cpu)
      resched |= c->handle_drq();

    if (Sched_context::rq.current().schedule_in_progress)
      {
        if (   (c->state() & Thread_ready_mask)
            && !c->in_ready_list()
            && on_current_cpu)
          Sched_context::rq.current().ready_enqueue(c->sched());
      }
    else if (resched)
      c->schedule();
  }

  static void
  force_to_invalid_cpu(Context *c)
  {
    // make sure this thread really never runs again by migrating it
    // to the 'invalid' CPU forcefully.
    Queue &q = Context::_pending_rqq.current();

      {
        auto g = lock_guard(q.q_lock());
        c->set_home_cpu(Cpu::invalid());
        if (c->_pending_rq.queued())
          q.dequeue(&c->_pending_rq);
      }
    c->handle_drq();
  }
};
